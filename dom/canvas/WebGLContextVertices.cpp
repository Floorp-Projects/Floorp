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
#include "WebGLTypes.h"
#include "WebGLVertexArray.h"

#include "mozilla/ResultVariant.h"

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

////////////////////////////////////////

void WebGLContext::VertexAttrib4T(GLuint index, const webgl::TypedQuad& src) {
  const FuncScope funcScope(*this, "vertexAttrib[1234]u?[fi]v?");
  if (IsContextLost()) return;

  if (!ValidateAttribIndex(*this, index)) return;

  ////

  if (index || !gl->IsCompatibilityProfile()) {
    switch (src.type) {
      case webgl::AttribBaseType::Boolean:
      case webgl::AttribBaseType::Float:
        gl->fVertexAttrib4fv(index,
                             reinterpret_cast<const float*>(src.data.data()));
        break;
      case webgl::AttribBaseType::Int:
        gl->fVertexAttribI4iv(
            index, reinterpret_cast<const int32_t*>(src.data.data()));
        break;
      case webgl::AttribBaseType::Uint:
        gl->fVertexAttribI4uiv(
            index, reinterpret_cast<const uint32_t*>(src.data.data()));
        break;
    }
  }

  ////

  mGenericVertexAttribTypes[index] = src.type;
  mGenericVertexAttribTypeInvalidator.InvalidateCaches();

  if (!index) {
    memcpy(mGenericVertexAttrib0Data, src.data.data(),
           sizeof(mGenericVertexAttrib0Data));
  }
}

////////////////////////////////////////

void WebGLContext::EnableVertexAttribArray(GLuint index) {
  const FuncScope funcScope(*this, "enableVertexAttribArray");
  if (IsContextLost()) return;

  if (!ValidateAttribIndex(*this, index)) return;

  gl->fEnableVertexAttribArray(index);

  MOZ_ASSERT(mBoundVertexArray);
  mBoundVertexArray->SetAttribIsArray(index, true);
}

void WebGLContext::DisableVertexAttribArray(GLuint index) {
  const FuncScope funcScope(*this, "disableVertexAttribArray");
  if (IsContextLost()) return;

  if (!ValidateAttribIndex(*this, index)) return;

  if (index || !gl->IsCompatibilityProfile()) {
    gl->fDisableVertexAttribArray(index);
  }

  MOZ_ASSERT(mBoundVertexArray);
  mBoundVertexArray->SetAttribIsArray(index, false);
}

Maybe<double> WebGLContext::GetVertexAttrib(GLuint index, GLenum pname) {
  const FuncScope funcScope(*this, "getVertexAttrib");
  if (IsContextLost()) return Nothing();

  if (!ValidateAttribIndex(*this, index)) return Nothing();

  MOZ_ASSERT(mBoundVertexArray);
  auto ret = mBoundVertexArray->GetVertexAttrib(index, pname);

  switch (pname) {
    case LOCAL_GL_VERTEX_ATTRIB_ARRAY_INTEGER:
      if (!IsWebGL2()) {
        ret = Nothing();
      }
      break;

    case LOCAL_GL_VERTEX_ATTRIB_ARRAY_DIVISOR:
      if (!IsWebGL2() &&
          !IsExtensionEnabled(WebGLExtensionID::ANGLE_instanced_arrays)) {
        ret = Nothing();
      }
      break;
  }

  if (!ret) {
    ErrorInvalidEnumInfo("pname", pname);
  }
  return ret;
}

////////////////////////////////////////

Result<webgl::VertAttribPointerCalculated, webgl::ErrorInfo>
CheckVertexAttribPointer(const bool isWebgl2,
                         const webgl::VertAttribPointerDesc& desc) {
  if (desc.channels < 1 || desc.channels > 4) {
    return Err(webgl::ErrorInfo{LOCAL_GL_INVALID_VALUE,
                                "Channel count `size` must be within [1,4]."});
  }

  ////

  webgl::VertAttribPointerCalculated calc;

  bool isTypeValid = true;
  bool isPackedType = false;
  uint8_t bytesPerType = 0;
  switch (desc.type) {
    // WebGL 1:
    case LOCAL_GL_BYTE:
      bytesPerType = 1;
      calc.baseType = webgl::AttribBaseType::Int;
      break;
    case LOCAL_GL_UNSIGNED_BYTE:
      bytesPerType = 1;
      calc.baseType = webgl::AttribBaseType::Uint;
      break;

    case LOCAL_GL_SHORT:
      bytesPerType = 2;
      calc.baseType = webgl::AttribBaseType::Int;
      break;
    case LOCAL_GL_UNSIGNED_SHORT:
      bytesPerType = 2;
      calc.baseType = webgl::AttribBaseType::Uint;
      break;

    case LOCAL_GL_FLOAT:
      bytesPerType = 4;
      calc.baseType = webgl::AttribBaseType::Float;
      break;

    // WebGL 2:
    case LOCAL_GL_INT:
      isTypeValid = isWebgl2;
      bytesPerType = 4;
      calc.baseType = webgl::AttribBaseType::Int;
      break;
    case LOCAL_GL_UNSIGNED_INT:
      isTypeValid = isWebgl2;
      bytesPerType = 4;
      calc.baseType = webgl::AttribBaseType::Uint;
      break;

    case LOCAL_GL_HALF_FLOAT:
      isTypeValid = isWebgl2;
      bytesPerType = 2;
      calc.baseType = webgl::AttribBaseType::Float;
      break;

    case LOCAL_GL_INT_2_10_10_10_REV:
    case LOCAL_GL_UNSIGNED_INT_2_10_10_10_REV:
      if (desc.channels != 4) {
        return Err(webgl::ErrorInfo{LOCAL_GL_INVALID_OPERATION,
                                    "Size must be 4 for this type."});
      }
      isTypeValid = isWebgl2;
      bytesPerType = 4;
      calc.baseType =
          webgl::AttribBaseType::Float;  // Invalid for intFunc:true.
      isPackedType = true;
      break;

    default:
      isTypeValid = false;
      break;
  }
  if (desc.intFunc) {
    isTypeValid = (calc.baseType != webgl::AttribBaseType::Float);
  } else {
    calc.baseType = webgl::AttribBaseType::Float;
  }
  if (!isTypeValid) {
    const auto info =
        nsPrintfCString("Bad `type`: %s", EnumString(desc.type).c_str());
    return Err(webgl::ErrorInfo{LOCAL_GL_INVALID_ENUM, info.BeginReading()});
  }

  ////

  calc.byteSize = bytesPerType;
  if (!isPackedType) {
    calc.byteSize *= desc.channels;
  }

  calc.byteStride =
      desc.byteStrideOrZero ? desc.byteStrideOrZero : calc.byteSize;

  // `alignment` should always be a power of two.
  MOZ_ASSERT(IsPowerOfTwo(bytesPerType));
  const auto typeAlignmentMask = bytesPerType - 1;

  if (calc.byteStride & typeAlignmentMask ||
      desc.byteOffset & typeAlignmentMask) {
    return Err(
        webgl::ErrorInfo{LOCAL_GL_INVALID_OPERATION,
                         "`stride` and `byteOffset` must satisfy the alignment"
                         " requirement of `type`."});
  }

  return calc;
}

void DoVertexAttribPointer(gl::GLContext& gl, const uint32_t index,
                           const webgl::VertAttribPointerDesc& desc) {
  if (desc.intFunc) {
    gl.fVertexAttribIPointer(index, desc.channels, desc.type,
                             desc.byteStrideOrZero,
                             reinterpret_cast<const void*>(desc.byteOffset));
  } else {
    gl.fVertexAttribPointer(index, desc.channels, desc.type, desc.normalized,
                            desc.byteStrideOrZero,
                            reinterpret_cast<const void*>(desc.byteOffset));
  }
}

void WebGLContext::VertexAttribPointer(
    const uint32_t index, const webgl::VertAttribPointerDesc& desc) {
  if (IsContextLost()) return;
  if (!ValidateAttribIndex(*this, index)) return;

  const auto res = CheckVertexAttribPointer(IsWebGL2(), desc);
  if (res.isErr()) {
    const auto& err = res.inspectErr();
    GenerateError(err.type, "%s", err.info.c_str());
    return;
  }
  const auto& calc = res.inspect();

  ////

  const auto& buffer = mBoundArrayBuffer;

  mBoundVertexArray->AttribPointer(index, buffer, desc, calc);

  const ScopedLazyBind lazyBind(gl, LOCAL_GL_ARRAY_BUFFER, buffer);
  DoVertexAttribPointer(*gl, index, desc);
}

////////////////////////////////////////

void WebGLContext::VertexAttribDivisor(GLuint index, GLuint divisor) {
  const FuncScope funcScope(*this, "vertexAttribDivisor");
  if (IsContextLost()) return;

  if (!ValidateAttribIndex(*this, index)) return;

  MOZ_ASSERT(mBoundVertexArray);
  mBoundVertexArray->AttribDivisor(index, divisor);
  gl->fVertexAttribDivisor(index, divisor);
}

}  // namespace mozilla
