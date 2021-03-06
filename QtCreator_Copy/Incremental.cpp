// -------------------- C++
#include <array>
#include <iostream>
#include "Incremental.h"

Incremental::Incremental( const TriMesh& _mesh )
    :mInputMesh(_mesh)
{
    incremental();
}

TriMesh Incremental::getResult() const
{
    return mHullMesh;
}

void Incremental::createInitialTetrahedron()
{
    // create the initial hull (the processed point will be marked as tagged)
    // initial face
    for (const auto& vh : mInputMesh.fv_range(OpenMesh::FaceHandle(0)))
    {
        mInputMesh.status(vh).set_tagged(true);
        mHullMesh.add_vertex(mInputMesh.point(vh));
    }
    // getting the distance by which the face is displaced
    auto normal = mInputMesh.calc_face_normal(OpenMesh::FaceHandle(0));
    auto dist = -1.0f * OpenMesh::dot(normal, mHullMesh.point(OpenMesh::VertexHandle(0)));

    OpenMesh::VertexHandle maxVh;
    auto maxDistance = 0.0f;
    // getting the furthest point from the initial face to construct the initial tetrahedron
    for (const auto& vh : mInputMesh.vertices())
    {
        auto distance = std::fabs(OpenMesh::dot(normal, mInputMesh.point(vh)) + dist);
        if (distance > maxDistance)
        {
            maxDistance = distance;
            maxVh = vh;
        }
    }
    // adding the point that makes the largest possible tetrahedron with the initial face
    mInputMesh.status(maxVh).set_tagged(true);
    mHullMesh.add_vertex(mInputMesh.point(maxVh));

    // triangulate the hull between the fourth point and initial face
    mHullMesh.add_face(OpenMesh::VertexHandle(0), OpenMesh::VertexHandle(1), OpenMesh::VertexHandle(2));
    mHullMesh.add_face(OpenMesh::VertexHandle(3), OpenMesh::VertexHandle(0), OpenMesh::VertexHandle(2));
    mHullMesh.add_face(OpenMesh::VertexHandle(2), OpenMesh::VertexHandle(1), OpenMesh::VertexHandle(3));
    mHullMesh.add_face(OpenMesh::VertexHandle(3), OpenMesh::VertexHandle(1), OpenMesh::VertexHandle(0));
}

void Incremental::incremental()
{
    mInputMesh.request_vertex_status();

    createInitialTetrahedron();

    mHullMesh.request_vertex_status();
    mHullMesh.request_halfedge_status();
    mHullMesh.request_face_status();
    mHullMesh.request_edge_status();

    // get a point that doesn't lie on the same plane as the face
    for (const auto& vh : mInputMesh.vertices())
    {
        std::vector<OpenMesh::FaceHandle> visibleFaces;
        visibleFaces.clear();
        if (!mInputMesh.status(vh).tagged())
        {
            mInputMesh.status(vh).set_tagged(true);

            for(const auto& fh : mHullMesh.faces())
            {
                //calculating volume
                std::array<OpenMesh::Vec3f, 3> tetrahedronEdges;
                int j = 0;
                auto point = mInputMesh.point(vh);

                //for(auto itr = hull.fv_ccwbegin(fh); itr != hull.fv_ccwend(fh); ++itr)
                for (const auto& fvh : mHullMesh.fv_range(fh))
                    tetrahedronEdges[j++] = mHullMesh.point(fvh) - point;

                // check if the tetrahedron volume is +ve to make sure it is outside the hull mesh
                if (OpenMesh::dot(tetrahedronEdges[2], OpenMesh::cross(tetrahedronEdges[1], tetrahedronEdges[0])) > (std::numeric_limits<float>::epsilon()))
                    visibleFaces.push_back(fh);
            }


            for (const auto&fh : visibleFaces)
            {
                for(const auto&he : mHullMesh.fh_range(fh))
                    mHullMesh.status(he).set_tagged(true);
                mHullMesh.delete_face(fh, true);
            }
            mHullMesh.garbage_collection();


            if (!visibleFaces.empty())
            {
                //add the point to hull and form cone faces and delete previously visible faces
                auto hullVh = mHullMesh.add_vertex(mInputMesh.point(vh));

                for(auto heh : mHullMesh.halfedges())
                {
                    if(mHullMesh.is_boundary(heh) || mHullMesh.status(heh).tagged())
                    {
                        mHullMesh.status(heh).set_tagged(false);
                        auto fromVh = mHullMesh.from_vertex_handle(heh);
                        auto toVh = mHullMesh.to_vertex_handle(heh);
                        /*if(OpenMesh::cross(mHullMesh.point(toVh) - mHullMesh.point(fromVh),
                                           mHullMesh.point(fromVh) -mHullMesh.point(hullVh)).norm()  > (std::numeric_limits<float>::epsilon()))
                        */
                        mHullMesh.add_face(fromVh, toVh, hullVh);
                    }
                }
            }
        }
    }

    mInputMesh.release_vertex_status();
    mInputMesh.release_halfedge_status();
    mHullMesh.release_face_status();
    mHullMesh.release_vertex_status();
    mHullMesh.release_edge_status();
}
