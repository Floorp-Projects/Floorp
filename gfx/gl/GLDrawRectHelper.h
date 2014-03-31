/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GLDRAWRECTHELPER_H_
#define GLDRAWRECTHELPER_H_

#include "GLContextTypes.h"
#include "GLConsts.h"

namespace mozilla {
namespace gl {

class GLContext;
class RectTriangles;

/** Helper to draw rectangles to the frame buffer. */
class GLDrawRectHelper MOZ_FINAL
{
public:
    GLDrawRectHelper(GLContext* aGL);
    ~GLDrawRectHelper();

    void DrawRect(GLuint aVertAttribIndex,
                  GLuint aTexCoordAttribIndex);
    void DrawRects(GLuint aVertAttribIndex,
                   GLuint aTexCoordAttribIndex,
                   RectTriangles& aRects);

private:
    // The GLContext is the sole owner of the GLDrawHelper.
    GLContext* mGL;
    GLuint mQuadVBO;
};

}
}

#endif // GLDRAWRECTHELPER_H_
