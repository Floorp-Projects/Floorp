/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL2Context.h"

#include "GLContext.h"
#include "js/Array.h"  // JS::NewArrayObject
#include "mozilla/dom/WebGL2RenderingContextBinding.h"
#include "mozilla/RefPtr.h"
#include "WebGLBuffer.h"
#include "WebGLContext.h"
#include "WebGLProgram.h"
#include "WebGLUniformLocation.h"
#include "WebGLVertexArray.h"
#include "WebGLVertexAttribData.h"

namespace mozilla {

// -------------------------------------------------------------------------
// Uniforms

void WebGLContext::Uniform1ui(WebGLUniformLocation* loc, GLuint v0) {
  const FuncScope funcScope(*this, "uniform1ui");
  if (!ValidateUniformSetter(loc, 1, webgl::AttribBaseType::UInt)) return;

  gl->fUniform1ui(loc->mLoc, v0);
}

void WebGLContext::Uniform2ui(WebGLUniformLocation* loc, GLuint v0, GLuint v1) {
  const FuncScope funcScope(*this, "uniform2ui");
  if (!ValidateUniformSetter(loc, 2, webgl::AttribBaseType::UInt)) return;

  gl->fUniform2ui(loc->mLoc, v0, v1);
}

void WebGLContext::Uniform3ui(WebGLUniformLocation* loc, GLuint v0, GLuint v1,
                              GLuint v2) {
  const FuncScope funcScope(*this, "uniform3ui");
  if (!ValidateUniformSetter(loc, 3, webgl::AttribBaseType::UInt)) return;

  gl->fUniform3ui(loc->mLoc, v0, v1, v2);
}

void WebGLContext::Uniform4ui(WebGLUniformLocation* loc, GLuint v0, GLuint v1,
                              GLuint v2, GLuint v3) {
  const FuncScope funcScope(*this, "uniform4ui");
  if (!ValidateUniformSetter(loc, 4, webgl::AttribBaseType::UInt)) return;

  gl->fUniform4ui(loc->mLoc, v0, v1, v2, v3);
}

// -------------------------------------------------------------------------
// Uniform Buffer Objects and Transform Feedback Buffers

MaybeWebGLVariant WebGL2Context::GetIndexedParameter(GLenum target,
                                                     GLuint index) {
  const FuncScope funcScope(*this, "getIndexedParameter");
  MaybeWebGLVariant ret;
  if (IsContextLost()) return ret;

  const std::vector<IndexedBufferBinding>* bindings;
  switch (target) {
    case LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
    case LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER_START:
    case LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER_SIZE:
      bindings = &(mBoundTransformFeedback->mIndexedBindings);
      break;

    case LOCAL_GL_UNIFORM_BUFFER_BINDING:
    case LOCAL_GL_UNIFORM_BUFFER_START:
    case LOCAL_GL_UNIFORM_BUFFER_SIZE:
      bindings = &mIndexedUniformBufferBindings;
      break;

    default:
      ErrorInvalidEnumInfo("target", target);
      return ret;
  }

  if (index >= bindings->size()) {
    ErrorInvalidValue("`index` must be < %s.",
                      "MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS");
    return ret;
  }
  const auto& binding = (*bindings)[index];

  switch (target) {
    case LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
    case LOCAL_GL_UNIFORM_BUFFER_BINDING:
      if (binding.mBufferBinding) {
        ret = AsSomeVariant(binding.mBufferBinding.get());
      }
      break;

    case LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER_START:
    case LOCAL_GL_UNIFORM_BUFFER_START:
      ret = AsSomeVariant(binding.mRangeStart);
      break;

    case LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER_SIZE:
    case LOCAL_GL_UNIFORM_BUFFER_SIZE:
      ret = AsSomeVariant(binding.mRangeSize);
      break;
  }

  return ret;
}

MaybeWebGLVariant WebGL2Context::GetUniformIndices(
    const WebGLProgram& program, const nsTArray<nsString>& uniformNames) {
  const FuncScope funcScope(*this, "getUniformIndices");
  if (IsContextLost()) return Nothing();

  if (!ValidateObject("program", program)) return Nothing();

  if (!uniformNames.Length()) return Nothing();

  return program.GetUniformIndices(uniformNames);
}

MaybeWebGLVariant WebGL2Context::GetActiveUniforms(
    const WebGLProgram& program, const nsTArray<GLuint>& uniformIndices,
    GLenum pname) {
  const FuncScope funcScope(*this, "getActiveUniforms");
  if (IsContextLost()) return Nothing();

  switch (pname) {
    case LOCAL_GL_UNIFORM_TYPE:
    case LOCAL_GL_UNIFORM_SIZE:
    case LOCAL_GL_UNIFORM_BLOCK_INDEX:
    case LOCAL_GL_UNIFORM_OFFSET:
    case LOCAL_GL_UNIFORM_ARRAY_STRIDE:
    case LOCAL_GL_UNIFORM_MATRIX_STRIDE:
    case LOCAL_GL_UNIFORM_IS_ROW_MAJOR:
      break;

    default:
      ErrorInvalidEnumInfo("pname", pname);
      return Nothing();
  }

  if (!ValidateObject("program", program)) return Nothing();

  if (!program.IsLinked()) {
    ErrorInvalidOperation("`program` must be linked.");
    return Nothing();
  }

  const auto& numActiveUniforms = program.LinkInfo()->uniforms.size();
  for (const auto& curIndex : uniformIndices) {
    if (curIndex >= numActiveUniforms) {
      ErrorInvalidValue("Too-large active uniform index queried.");
      return Nothing();
    }
  }

  const auto& count = uniformIndices.Length();

  MaybeWebGLVariant ret;
  switch (pname) {
    case LOCAL_GL_UNIFORM_TYPE:
    case LOCAL_GL_UNIFORM_SIZE:
    case LOCAL_GL_UNIFORM_BLOCK_INDEX:
    case LOCAL_GL_UNIFORM_OFFSET:
    case LOCAL_GL_UNIFORM_ARRAY_STRIDE:
    case LOCAL_GL_UNIFORM_MATRIX_STRIDE: {
      ret = AsSomeVariant(nsTArray<int32_t>());
      nsTArray<int32_t>& variantArray = ret.ref().as<nsTArray<int32_t>>();
      GLint* array = variantArray.AppendElements(count);
      if (!array) {
        ErrorOutOfMemory("Failed to allocate return array.");
        return Nothing();
      }

      gl->fGetActiveUniformsiv(program.mGLName, count,
                               uniformIndices.Elements(), pname, array);
    } break;
    case LOCAL_GL_UNIFORM_IS_ROW_MAJOR: {
      ret = AsSomeVariant(nsTArray<bool>());
      nsTArray<bool>& variantArray = ret.ref().as<nsTArray<bool>>();
      GLint* intArray = new GLint[count];
      if (!intArray) {
        ErrorOutOfMemory("Failed to allocate int buffer.");
        return Nothing();
      }
      gl->fGetActiveUniformsiv(program.mGLName, count,
                               uniformIndices.Elements(), pname, intArray);
      bool* boolArray = variantArray.AppendElements(count);
      if (!boolArray) {
        ErrorOutOfMemory("Failed to allocate return array.");
        return Nothing();
      }
      for (uint32_t i = 0; i < count; ++i) {
        boolArray[i] = intArray[i];
      }
    } break;

    default:
      MOZ_CRASH("Invalid pname");
  }

  return ret;
}

GLuint WebGL2Context::GetUniformBlockIndex(const WebGLProgram& program,
                                           const nsAString& uniformBlockName) {
  const FuncScope funcScope(*this, "getUniformBlockIndex");
  if (IsContextLost()) return 0;

  if (!ValidateObject("program", program)) return 0;

  return program.GetUniformBlockIndex(uniformBlockName);
}

MaybeWebGLVariant WebGL2Context::GetActiveUniformBlockParameter(
    const WebGLProgram& program, GLuint uniformBlockIndex, GLenum pname) {
  const FuncScope funcScope(*this, "getActiveUniformBlockParameter");
  if (IsContextLost()) return Nothing();

  if (!ValidateObject("program", program)) return Nothing();

  switch (pname) {
    case LOCAL_GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER:
    case LOCAL_GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER:
    case LOCAL_GL_UNIFORM_BLOCK_BINDING:
    case LOCAL_GL_UNIFORM_BLOCK_DATA_SIZE:
    case LOCAL_GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS:
      return program.GetActiveUniformBlockParam(uniformBlockIndex, pname);

    case LOCAL_GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES:
      return program.GetActiveUniformBlockActiveUniforms(uniformBlockIndex);
  }

  ErrorInvalidEnumInfo("parameter", pname);
  return Nothing();
}

nsString WebGL2Context::GetActiveUniformBlockName(const WebGLProgram& program,
                                                  GLuint uniformBlockIndex) {
  const FuncScope funcScope(*this, "getActiveUniformBlockName");
  if (IsContextLost()) return EmptyString();

  if (!ValidateObject("program", program)) return EmptyString();

  return program.GetActiveUniformBlockName(uniformBlockIndex);
}

void WebGL2Context::UniformBlockBinding(WebGLProgram& program,
                                        GLuint uniformBlockIndex,
                                        GLuint uniformBlockBinding) {
  const FuncScope funcScope(*this, "uniformBlockBinding");
  if (IsContextLost()) return;

  if (!ValidateObject("program", program)) return;

  program.UniformBlockBinding(uniformBlockIndex, uniformBlockBinding);
}

}  // namespace mozilla
