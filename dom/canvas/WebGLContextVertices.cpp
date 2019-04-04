/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"

#include "GLContext.h"
#include "mozilla/Casting.h"
#include "mozilla/CheckedInt.h"
#include "WebGLBuffer.h"
#include "WebGLFramebuffer.h"
#include "WebGLProgram.h"
#include "WebGLRenderbuffer.h"
#include "WebGLShader.h"
#include "WebGLTexture.h"
#include "WebGLVertexArray.h"
#include "WebGLVertexAttribData.h"

#include "mozilla/Casting.h"

namespace mozilla {

static bool ValidateAttribIndex(WebGLContext& webgl, GLuint index) {
  bool valid = (index < webgl.MaxVertexAttribs());

  if (!valid) {
    if (index == GLuint(-1)) {
      webgl.ErrorInvalidValue(
          "-1 is not a valid `index`. This value"
          " probably comes from a getAttribLocation()"
          " call, where this return value -1 means"
          " that the passed name didn't correspond to"
          " an active attribute in the specified"
          " program.");
    } else {
      webgl.ErrorInvalidValue(
          "`index` must be less than"
          " MAX_VERTEX_ATTRIBS.");
    }
  }

  return valid;
}

JSObject* WebGLContext::GetVertexAttribFloat32Array(JSContext* cx,
                                                    GLuint index) {
  GLfloat attrib[4];
  if (index) {
    gl->fGetVertexAttribfv(index, LOCAL_GL_CURRENT_VERTEX_ATTRIB, attrib);
  } else {
    memcpy(attrib, mGenericVertexAttrib0Data,
           sizeof(mGenericVertexAttrib0Data));
  }
  return dom::Float32Array::Create(cx, this, 4, attrib);
}

JSObject* WebGLContext::GetVertexAttribInt32Array(JSContext* cx, GLuint index) {
  GLint attrib[4];
  if (index) {
    gl->fGetVertexAttribIiv(index, LOCAL_GL_CURRENT_VERTEX_ATTRIB, attrib);
  } else {
    memcpy(attrib, mGenericVertexAttrib0Data,
           sizeof(mGenericVertexAttrib0Data));
  }
  return dom::Int32Array::Create(cx, this, 4, attrib);
}

JSObject* WebGLContext::GetVertexAttribUint32Array(JSContext* cx,
                                                   GLuint index) {
  GLuint attrib[4];
  if (index) {
    gl->fGetVertexAttribIuiv(index, LOCAL_GL_CURRENT_VERTEX_ATTRIB, attrib);
  } else {
    memcpy(attrib, mGenericVertexAttrib0Data,
           sizeof(mGenericVertexAttrib0Data));
  }
  return dom::Uint32Array::Create(cx, this, 4, attrib);
}

////////////////////////////////////////

void WebGLContext::VertexAttrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z,
                                  GLfloat w) {
  const FuncScope funcScope(*this, "vertexAttrib4f");
  if (IsContextLost()) return;

  if (!ValidateAttribIndex(*this, index)) return;

  ////

  if (index || !gl->IsCompatibilityProfile()) {
    gl->fVertexAttrib4f(index, x, y, z, w);
  }

  ////

  mGenericVertexAttribTypes[index] = webgl::AttribBaseType::Float;
  mGenericVertexAttribTypeInvalidator.InvalidateCaches();

  if (!index) {
    const float data[4] = {x, y, z, w};
    memcpy(mGenericVertexAttrib0Data, data, sizeof(data));
  }
}

void WebGL2Context::VertexAttribI4i(GLuint index, GLint x, GLint y, GLint z,
                                    GLint w) {
  const FuncScope funcScope(*this, "vertexAttribI4i");
  if (IsContextLost()) return;

  if (!ValidateAttribIndex(*this, index)) return;

  ////

  if (index || !gl->IsCompatibilityProfile()) {
    gl->fVertexAttribI4i(index, x, y, z, w);
  }

  ////

  mGenericVertexAttribTypes[index] = webgl::AttribBaseType::Int;
  mGenericVertexAttribTypeInvalidator.InvalidateCaches();

  if (!index) {
    const int32_t data[4] = {x, y, z, w};
    memcpy(mGenericVertexAttrib0Data, data, sizeof(data));
  }
}

void WebGL2Context::VertexAttribI4ui(GLuint index, GLuint x, GLuint y, GLuint z,
                                     GLuint w) {
  const FuncScope funcScope(*this, "vertexAttribI4ui");
  if (IsContextLost()) return;

  if (!ValidateAttribIndex(*this, index)) return;

  ////

  if (index || !gl->IsCompatibilityProfile()) {
    gl->fVertexAttribI4ui(index, x, y, z, w);
  }

  ////

  mGenericVertexAttribTypes[index] = webgl::AttribBaseType::UInt;
  mGenericVertexAttribTypeInvalidator.InvalidateCaches();

  if (!index) {
    const uint32_t data[4] = {x, y, z, w};
    memcpy(mGenericVertexAttrib0Data, data, sizeof(data));
  }
}

////////////////////////////////////////

void WebGLContext::EnableVertexAttribArray(GLuint index) {
  const FuncScope funcScope(*this, "enableVertexAttribArray");
  if (IsContextLost()) return;

  if (!ValidateAttribIndex(*this, index)) return;

  gl->fEnableVertexAttribArray(index);

  MOZ_ASSERT(mBoundVertexArray);
  mBoundVertexArray->mAttribs[index].mEnabled = true;
  mBoundVertexArray->InvalidateCaches();
}

void WebGLContext::DisableVertexAttribArray(GLuint index) {
  const FuncScope funcScope(*this, "disableVertexAttribArray");
  if (IsContextLost()) return;

  if (!ValidateAttribIndex(*this, index)) return;

  if (index || !gl->IsCompatibilityProfile()) {
    gl->fDisableVertexAttribArray(index);
  }

  MOZ_ASSERT(mBoundVertexArray);
  mBoundVertexArray->mAttribs[index].mEnabled = false;
  mBoundVertexArray->InvalidateCaches();
}

JS::Value WebGLContext::GetVertexAttrib(JSContext* cx, GLuint index,
                                        GLenum pname, ErrorResult& rv) {
  const FuncScope funcScope(*this, "getVertexAttrib");
  if (IsContextLost()) return JS::NullValue();

  if (!ValidateAttribIndex(*this, index)) return JS::NullValue();

  MOZ_ASSERT(mBoundVertexArray);

  switch (pname) {
    case LOCAL_GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING:
      return WebGLObjectAsJSValue(
          cx, mBoundVertexArray->mAttribs[index].mBuf.get(), rv);

    case LOCAL_GL_VERTEX_ATTRIB_ARRAY_STRIDE:
      return JS::Int32Value(mBoundVertexArray->mAttribs[index].Stride());

    case LOCAL_GL_VERTEX_ATTRIB_ARRAY_SIZE:
      return JS::Int32Value(mBoundVertexArray->mAttribs[index].Size());

    case LOCAL_GL_VERTEX_ATTRIB_ARRAY_TYPE:
      return JS::Int32Value(mBoundVertexArray->mAttribs[index].Type());

    case LOCAL_GL_VERTEX_ATTRIB_ARRAY_INTEGER:
      if (IsWebGL2())
        return JS::BooleanValue(
            mBoundVertexArray->mAttribs[index].IntegerFunc());

      break;

    case LOCAL_GL_VERTEX_ATTRIB_ARRAY_DIVISOR:
      if (IsWebGL2() ||
          IsExtensionEnabled(WebGLExtensionID::ANGLE_instanced_arrays)) {
        return JS::Int32Value(mBoundVertexArray->mAttribs[index].mDivisor);
      }
      break;

    case LOCAL_GL_CURRENT_VERTEX_ATTRIB: {
      JS::RootedObject obj(cx);
      switch (mGenericVertexAttribTypes[index]) {
        case webgl::AttribBaseType::Float:
          obj = GetVertexAttribFloat32Array(cx, index);
          break;

        case webgl::AttribBaseType::Int:
          obj = GetVertexAttribInt32Array(cx, index);
          break;

        case webgl::AttribBaseType::UInt:
          obj = GetVertexAttribUint32Array(cx, index);
          break;

        case webgl::AttribBaseType::Boolean:
          MOZ_CRASH("impossible");
      }

      if (!obj) {
        rv.Throw(NS_ERROR_OUT_OF_MEMORY);
        return JS::NullValue();
      }
      return JS::ObjectValue(*obj);
    }

    case LOCAL_GL_VERTEX_ATTRIB_ARRAY_ENABLED:
      return JS::BooleanValue(mBoundVertexArray->mAttribs[index].mEnabled);

    case LOCAL_GL_VERTEX_ATTRIB_ARRAY_NORMALIZED:
      return JS::BooleanValue(mBoundVertexArray->mAttribs[index].Normalized());

    default:
      break;
  }

  ErrorInvalidEnumInfo("pname", pname);
  return JS::NullValue();
}

WebGLsizeiptr WebGLContext::GetVertexAttribOffset(GLuint index, GLenum pname) {
  const FuncScope funcScope(*this, "getVertexAttribOffset");
  if (IsContextLost()) return 0;

  if (!ValidateAttribIndex(*this, index)) return 0;

  if (pname != LOCAL_GL_VERTEX_ATTRIB_ARRAY_POINTER) {
    ErrorInvalidEnum("`pname` must be VERTEX_ATTRIB_ARRAY_POINTER.");
    return 0;
  }

  MOZ_ASSERT(mBoundVertexArray);
  return mBoundVertexArray->mAttribs[index].ByteOffset();
}

////////////////////////////////////////

void WebGLContext::VertexAttribAnyPointer(bool isFuncInt, GLuint index,
                                          GLint size, GLenum type,
                                          bool normalized, GLsizei stride,
                                          WebGLintptr byteOffset) {
  if (IsContextLost()) return;

  if (!ValidateAttribIndex(*this, index)) return;

  ////

  if (size < 1 || size > 4) {
    ErrorInvalidValue("Invalid element size.");
    return;
  }

  // see WebGL spec section 6.6 "Vertex Attribute Data Stride"
  if (stride < 0 || stride > 255) {
    ErrorInvalidValue("Negative or too large stride.");
    return;
  }

  if (byteOffset < 0) {
    ErrorInvalidValue("Negative offset.");
    return;
  }

  ////

  bool isTypeValid = true;
  uint8_t typeAlignment;
  switch (type) {
    // WebGL 1:
    case LOCAL_GL_BYTE:
    case LOCAL_GL_UNSIGNED_BYTE:
      typeAlignment = 1;
      break;

    case LOCAL_GL_SHORT:
    case LOCAL_GL_UNSIGNED_SHORT:
      typeAlignment = 2;
      break;

    case LOCAL_GL_FLOAT:
      if (isFuncInt) {
        isTypeValid = false;
      }
      typeAlignment = 4;
      break;

    // WebGL 2:
    case LOCAL_GL_INT:
    case LOCAL_GL_UNSIGNED_INT:
      if (!IsWebGL2()) {
        isTypeValid = false;
      }
      typeAlignment = 4;
      break;

    case LOCAL_GL_HALF_FLOAT:
      if (isFuncInt || !IsWebGL2()) {
        isTypeValid = false;
      }
      typeAlignment = 2;
      break;

    case LOCAL_GL_FIXED:
      if (isFuncInt || !IsWebGL2()) {
        isTypeValid = false;
      }
      typeAlignment = 4;
      break;

    case LOCAL_GL_INT_2_10_10_10_REV:
    case LOCAL_GL_UNSIGNED_INT_2_10_10_10_REV:
      if (isFuncInt || !IsWebGL2()) {
        isTypeValid = false;
        break;
      }
      if (size != 4) {
        ErrorInvalidOperation("Size must be 4 for this type.");
        return;
      }
      typeAlignment = 4;
      break;

    default:
      isTypeValid = false;
      break;
  }
  if (!isTypeValid) {
    ErrorInvalidEnumInfo("type", type);
    return;
  }

  ////

  // `alignment` should always be a power of two.
  MOZ_ASSERT(IsPowerOfTwo(typeAlignment));
  const GLsizei typeAlignmentMask = typeAlignment - 1;

  if (stride & typeAlignmentMask || byteOffset & typeAlignmentMask) {
    ErrorInvalidOperation(
        "`stride` and `byteOffset` must satisfy the alignment"
        " requirement of `type`.");
    return;
  }

  ////

  const auto& buffer = mBoundArrayBuffer;
  if (!buffer && byteOffset) {
    ErrorInvalidOperation("If ARRAY_BUFFER is null, byteOffset must be zero.");
    return;
  }

  ////

  if (isFuncInt) {
    gl->fVertexAttribIPointer(index, size, type, stride,
                              reinterpret_cast<void*>(byteOffset));
  } else {
    gl->fVertexAttribPointer(index, size, type, normalized, stride,
                             reinterpret_cast<void*>(byteOffset));
  }

  WebGLVertexAttribData& vd = mBoundVertexArray->mAttribs[index];
  vd.VertexAttribPointer(isFuncInt, buffer, AutoAssertCast(size), type,
                         normalized, stride, byteOffset);
  mBoundVertexArray->InvalidateCaches();
}

////////////////////////////////////////

void WebGLContext::VertexAttribDivisor(GLuint index, GLuint divisor) {
  const FuncScope funcScope(*this, "vertexAttribDivisor");
  if (IsContextLost()) return;

  if (!ValidateAttribIndex(*this, index)) return;

  MOZ_ASSERT(mBoundVertexArray);
  mBoundVertexArray->mAttribs[index].mDivisor = divisor;
  mBoundVertexArray->InvalidateCaches();

  gl->fVertexAttribDivisor(index, divisor);
}

}  // namespace mozilla
