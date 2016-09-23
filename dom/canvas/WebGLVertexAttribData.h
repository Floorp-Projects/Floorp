/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_VERTEX_ATTRIB_DATA_H_
#define WEBGL_VERTEX_ATTRIB_DATA_H_

#include "GLDefs.h"
#include "WebGLObjectModel.h"

namespace mozilla {

class WebGLBuffer;

struct WebGLVertexAttribData
{
    // note that these initial values are what GL initializes vertex attribs to
    WebGLVertexAttribData()
        : buf(0)
        , stride(0)
        , size(4)
        , divisor(0) // OpenGL ES 3.0 specs paragraphe 6.2 p240
        , byteOffset(0)
        , type(LOCAL_GL_FLOAT)
        , enabled(false)
        , normalized(false)
        , integer(false)
    {}

    WebGLRefPtr<WebGLBuffer> buf;
    GLuint stride;
    GLuint size;
    GLuint divisor;
    GLuint byteOffset;
    GLenum type;
    bool enabled;
    bool normalized;
    bool integer;

    GLuint componentSize() const {
        switch(type) {
        case LOCAL_GL_BYTE:
        case LOCAL_GL_UNSIGNED_BYTE:
            return 1;

        case LOCAL_GL_SHORT:
        case LOCAL_GL_UNSIGNED_SHORT:
        case LOCAL_GL_HALF_FLOAT:
        case LOCAL_GL_HALF_FLOAT_OES:
            return 2;

        case LOCAL_GL_INT:
        case LOCAL_GL_UNSIGNED_INT:
        case LOCAL_GL_FLOAT:
            return 4;

        default:
            MOZ_ASSERT(false, "Should never get here!");
            return 0;
        }
    }

    GLuint actualStride() const {
        if (stride)
            return stride;

        return size * componentSize();
    }
};

} // namespace mozilla

inline void
ImplCycleCollectionUnlink(mozilla::WebGLVertexAttribData& field)
{
    field.buf = nullptr;
}

inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& callback,
                            mozilla::WebGLVertexAttribData& field,
                            const char* name,
                            uint32_t flags = 0)
{
    CycleCollectionNoteChild(callback, field.buf.get(), name, flags);
}

#endif // WEBGL_VERTEX_ATTRIB_DATA_H_
