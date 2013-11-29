/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DecomposeIntoNoRepeatTriangles_h_
#define DecomposeIntoNoRepeatTriangles_h_

#include "GLTypes.h"
#include "nsRect.h"
#include "nsTArray.h"

namespace mozilla {
namespace gl {

/** Helper for DecomposeIntoNoRepeatTriangles
  */
class RectTriangles {
public:
    RectTriangles() { }

    // Always pass texture coordinates upright. If you want to flip the
    // texture coordinates emitted to the tex_coords array, set flip_y to
    // true.
    void addRect(GLfloat x0, GLfloat y0, GLfloat x1, GLfloat y1,
                  GLfloat tx0, GLfloat ty0, GLfloat tx1, GLfloat ty1,
                  bool flip_y = false);

    /**
      * these return a float pointer to the start of each array respectively.
      * Use it for glVertexAttribPointer calls.
      * We can return nullptr if we choose to use Vertex Buffer Objects here.
      */
    float* vertexPointer() {
        return &vertexCoords[0].x;
    }

    float* texCoordPointer() {
        return &texCoords[0].u;
    }

    unsigned int elements() {
        return vertexCoords.Length();
    }

    typedef struct { GLfloat x,y; } vert_coord;
    typedef struct { GLfloat u,v; } tex_coord;
private:
    // default is 4 rectangles, each made up of 2 triangles (3 coord vertices each)
    nsAutoTArray<vert_coord, 6> vertexCoords;
    nsAutoTArray<tex_coord, 6>  texCoords;
};

/**
  * Decompose drawing the possibly-wrapped aTexCoordRect rectangle
  * of a texture of aTexSize into one or more rectangles (represented
  * as 2 triangles) and associated tex coordinates, such that
  * we don't have to use the REPEAT wrap mode. If aFlipY is true, the
  * texture coordinates will be specified vertically flipped.
  *
  * The resulting triangle vertex coordinates will be in the space of
  * (0.0, 0.0) to (1.0, 1.0) -- transform the coordinates appropriately
  * if you need a different space.
  *
  * The resulting vertex coordinates should be drawn using GL_TRIANGLES,
  * and rects.numRects * 3 * 6
  */
void DecomposeIntoNoRepeatTriangles(const nsIntRect& aTexCoordRect,
                                    const nsIntSize& aTexSize,
                                    RectTriangles& aRects,
                                    bool aFlipY = false);

}
}

#endif // DecomposeIntoNoRepeatTriangles_h_