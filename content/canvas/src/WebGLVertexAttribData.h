/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLVERTEXATTRIBDATA_H_
#define WEBGLVERTEXATTRIBDATA_H_

namespace mozilla {

class WebGLBuffer;

struct WebGLVertexAttribData {
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
    { }

    WebGLRefPtr<WebGLBuffer> buf;
    GLuint stride;
    GLuint size;
    GLuint divisor;
    GLuint byteOffset;
    GLenum type;
    bool enabled;
    bool normalized;

    GLuint componentSize() const {
        switch(type) {
            case LOCAL_GL_BYTE:
                return sizeof(GLbyte);
                break;
            case LOCAL_GL_UNSIGNED_BYTE:
                return sizeof(GLubyte);
                break;
            case LOCAL_GL_SHORT:
                return sizeof(GLshort);
                break;
            case LOCAL_GL_UNSIGNED_SHORT:
                return sizeof(GLushort);
                break;
            // XXX case LOCAL_GL_FIXED:
            case LOCAL_GL_FLOAT:
                return sizeof(GLfloat);
                break;
            default:
                NS_ERROR("Should never get here!");
                return 0;
        }
    }

    GLuint actualStride() const {
        if (stride) return stride;
        return size * componentSize();
    }
};

} // namespace mozilla

inline void ImplCycleCollectionUnlink(mozilla::WebGLVertexAttribData& aField)
{
  aField.buf = nullptr;
}

inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            mozilla::WebGLVertexAttribData& aField,
                            const char* aName,
                            uint32_t aFlags = 0)
{
  CycleCollectionNoteChild(aCallback, aField.buf.get(), aName, aFlags);
}

#endif