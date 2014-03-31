/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLDrawRectHelper.h"
#include "GLContext.h"
#include "DecomposeIntoNoRepeatTriangles.h"

namespace mozilla {
namespace gl {

static GLfloat quad[] = {
    /* First quad vertices */
    0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
    /* Then quad texcoords */
    0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
};

#define BUFFER_OFFSET(i) ((char *)nullptr + (i))

GLDrawRectHelper::GLDrawRectHelper(GLContext *aGL)
    : mGL(aGL)
{
    mGL->fGenBuffers(1, &mQuadVBO);
    mGL->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, mQuadVBO);

    mGL->fBufferData(LOCAL_GL_ARRAY_BUFFER, sizeof(quad), quad, LOCAL_GL_STATIC_DRAW);
    mGL->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);
}

GLDrawRectHelper::~GLDrawRectHelper()
{
    mGL->fDeleteBuffers(1, &mQuadVBO);
}

void
GLDrawRectHelper::DrawRect(GLuint aVertAttribIndex,
                           GLuint aTexCoordAttribIndex)
{
    mGL->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, mQuadVBO);

    mGL->fVertexAttribPointer(aVertAttribIndex,
                              2, LOCAL_GL_FLOAT,
                              LOCAL_GL_FALSE,
                              0, BUFFER_OFFSET(0));
    mGL->fEnableVertexAttribArray(aVertAttribIndex);

    if (aTexCoordAttribIndex != GLuint(-1)) {
        mGL->fVertexAttribPointer(aTexCoordAttribIndex,
                                  2, LOCAL_GL_FLOAT,
                                  LOCAL_GL_FALSE,
                                  0, BUFFER_OFFSET(sizeof(quad)/2));
        mGL->fEnableVertexAttribArray(aTexCoordAttribIndex);
    }

    mGL->fDrawArrays(LOCAL_GL_TRIANGLE_STRIP, 0, 4);

    if (aTexCoordAttribIndex != GLuint(-1)) {
        mGL->fDisableVertexAttribArray(aTexCoordAttribIndex);
    }
    mGL->fDisableVertexAttribArray(aVertAttribIndex);

    mGL->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);
}

void
GLDrawRectHelper::DrawRects(GLuint aVertAttribIndex,
                            GLuint aTexCoordAttribIndex,
                            RectTriangles& aRects)
{
    GLsizei bytes = aRects.vertCoords().Length() * 2 * sizeof(GLfloat);
    GLsizei total = bytes;
    if (aTexCoordAttribIndex != GLuint(-1)) {
        total *= 2;
    }

    GLuint vbo;
    mGL->fGenBuffers(1, &vbo);
    mGL->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, vbo);
    mGL->fBufferData(LOCAL_GL_ARRAY_BUFFER,
                     total,
                     nullptr,
                     LOCAL_GL_STREAM_DRAW);

    mGL->fBufferSubData(LOCAL_GL_ARRAY_BUFFER,
                        0,
                        bytes,
                        aRects.vertCoords().Elements());
    mGL->fVertexAttribPointer(aVertAttribIndex,
                              2, LOCAL_GL_FLOAT,
                              LOCAL_GL_FALSE,
                              0, BUFFER_OFFSET(0));
    mGL->fEnableVertexAttribArray(aVertAttribIndex);

    if (aTexCoordAttribIndex != GLuint(-1)) {
        mGL->fBufferSubData(LOCAL_GL_ARRAY_BUFFER,
                            bytes,
                            bytes,
                            aRects.texCoords().Elements());
        mGL->fVertexAttribPointer(aTexCoordAttribIndex,
                                  2, LOCAL_GL_FLOAT,
                                  LOCAL_GL_FALSE,
                                  0, BUFFER_OFFSET(bytes));
        mGL->fEnableVertexAttribArray(aTexCoordAttribIndex);
    }

    mGL->fDrawArrays(LOCAL_GL_TRIANGLES, 0, aRects.vertCoords().Length());

    if (aTexCoordAttribIndex != GLuint(-1)) {
        mGL->fDisableVertexAttribArray(aTexCoordAttribIndex);
    }
    mGL->fDisableVertexAttribArray(aVertAttribIndex);

    mGL->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);

    mGL->fDeleteBuffers(1, &vbo);
}

}
}
