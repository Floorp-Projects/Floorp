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

class WebGLVertexAttribData final
{
public:
    uint32_t mDivisor;
    bool mEnabled;

private:
    bool mIntegerFunc;
public:
    WebGLRefPtr<WebGLBuffer> mBuf;
private:
    GLenum mType;
    uint8_t mSize; // num of mType vals per vert
    uint8_t mBytesPerVertex;
    bool mNormalized;
    uint32_t mStride; // bytes
    uint32_t mExplicitStride;
    uint64_t mByteOffset;

public:

#define GETTER(X) const decltype(m##X)& X() const { return m##X; }

    GETTER(IntegerFunc)
    GETTER(Type)
    GETTER(Size)
    GETTER(BytesPerVertex)
    GETTER(Normalized)
    GETTER(Stride)
    GETTER(ExplicitStride)
    GETTER(ByteOffset)

#undef GETTER

    // note that these initial values are what GL initializes vertex attribs to
    WebGLVertexAttribData()
        : mDivisor(0)
        , mEnabled(false)
    {
        VertexAttribPointer(false, nullptr, 4, LOCAL_GL_FLOAT, false, 0, 0);
    }

    void VertexAttribPointer(bool integerFunc, WebGLBuffer* buf, uint8_t size,
                             GLenum type, bool normalized, uint32_t stride,
                             uint64_t byteOffset);

    void DoVertexAttribPointer(gl::GLContext* gl, GLuint index) const;
};

} // namespace mozilla

inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& callback,
                            mozilla::WebGLVertexAttribData& field,
                            const char* name,
                            uint32_t flags = 0)
{
    CycleCollectionNoteChild(callback, field.mBuf.get(), name, flags);
}

#endif // WEBGL_VERTEX_ATTRIB_DATA_H_
