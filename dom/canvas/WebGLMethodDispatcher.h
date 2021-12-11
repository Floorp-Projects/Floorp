/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLMETHODDISPATCHER_H_
#define WEBGLMETHODDISPATCHER_H_

#include "TexUnpackBlob.h"
#include "HostWebGLContext.h"
#include "WebGLQueueParamTraits.h"

namespace mozilla {

template <size_t id = 0>
class WebGLMethodDispatcher
    : public EmptyMethodDispatcher<WebGLMethodDispatcher> {};

template <typename MethodT, MethodT Method>
size_t IdByMethod() = delete;

#define DEFINE_METHOD_DISPATCHER(_ID, _METHOD)                  \
  template <>                                                   \
  class WebGLMethodDispatcher<_ID>                              \
      : public MethodDispatcher<WebGLMethodDispatcher, _ID,     \
                                decltype(&_METHOD), &_METHOD> { \
   public:                                                      \
    static inline const char* Name() { return #_METHOD; }       \
  };                                                            \
  template <>                                                   \
  inline size_t IdByMethod<decltype(&_METHOD), &_METHOD>() {    \
    return _ID;                                                 \
  }

// Defines each method the WebGLMethodDispatcher handles.  The COUNTER value
// is used as a cross-process ID for each of the methods.
#define DEFINE_ASYNC(_METHOD) DEFINE_METHOD_DISPATCHER(__COUNTER__, _METHOD)

DEFINE_ASYNC(HostWebGLContext::CreateBuffer)
DEFINE_ASYNC(HostWebGLContext::CreateFramebuffer)
DEFINE_ASYNC(HostWebGLContext::CreateProgram)
DEFINE_ASYNC(HostWebGLContext::CreateQuery)
DEFINE_ASYNC(HostWebGLContext::CreateRenderbuffer)
DEFINE_ASYNC(HostWebGLContext::CreateSampler)
DEFINE_ASYNC(HostWebGLContext::CreateShader)
DEFINE_ASYNC(HostWebGLContext::CreateSync)
DEFINE_ASYNC(HostWebGLContext::CreateTexture)
DEFINE_ASYNC(HostWebGLContext::CreateTransformFeedback)
DEFINE_ASYNC(HostWebGLContext::CreateVertexArray)

DEFINE_ASYNC(HostWebGLContext::DeleteBuffer)
DEFINE_ASYNC(HostWebGLContext::DeleteFramebuffer)
DEFINE_ASYNC(HostWebGLContext::DeleteProgram)
DEFINE_ASYNC(HostWebGLContext::DeleteQuery)
DEFINE_ASYNC(HostWebGLContext::DeleteRenderbuffer)
DEFINE_ASYNC(HostWebGLContext::DeleteSampler)
DEFINE_ASYNC(HostWebGLContext::DeleteShader)
DEFINE_ASYNC(HostWebGLContext::DeleteSync)
DEFINE_ASYNC(HostWebGLContext::DeleteTexture)
DEFINE_ASYNC(HostWebGLContext::DeleteTransformFeedback)
DEFINE_ASYNC(HostWebGLContext::DeleteVertexArray)

DEFINE_ASYNC(HostWebGLContext::SetEnabled)
DEFINE_ASYNC(HostWebGLContext::GenerateError)
DEFINE_ASYNC(HostWebGLContext::Resize)
DEFINE_ASYNC(HostWebGLContext::RequestExtension)
DEFINE_ASYNC(HostWebGLContext::DidRefresh)
DEFINE_ASYNC(HostWebGLContext::AttachShader)
DEFINE_ASYNC(HostWebGLContext::BindAttribLocation)
DEFINE_ASYNC(HostWebGLContext::BindFramebuffer)
DEFINE_ASYNC(HostWebGLContext::BlendColor)
DEFINE_ASYNC(HostWebGLContext::BlendEquationSeparate)
DEFINE_ASYNC(HostWebGLContext::BlendFuncSeparate)
DEFINE_ASYNC(HostWebGLContext::Clear)
DEFINE_ASYNC(HostWebGLContext::ClearColor)
DEFINE_ASYNC(HostWebGLContext::ClearDepth)
DEFINE_ASYNC(HostWebGLContext::ClearStencil)
DEFINE_ASYNC(HostWebGLContext::ColorMask)
DEFINE_ASYNC(HostWebGLContext::CompileShader)
DEFINE_ASYNC(HostWebGLContext::CullFace)
DEFINE_ASYNC(HostWebGLContext::DepthFunc)
DEFINE_ASYNC(HostWebGLContext::DepthMask)
DEFINE_ASYNC(HostWebGLContext::DepthRange)
DEFINE_ASYNC(HostWebGLContext::DetachShader)
DEFINE_ASYNC(HostWebGLContext::Flush)
DEFINE_ASYNC(HostWebGLContext::FramebufferAttach)
DEFINE_ASYNC(HostWebGLContext::FrontFace)
DEFINE_ASYNC(HostWebGLContext::Hint)
DEFINE_ASYNC(HostWebGLContext::LineWidth)
DEFINE_ASYNC(HostWebGLContext::LinkProgram)
DEFINE_ASYNC(HostWebGLContext::PolygonOffset)
DEFINE_ASYNC(HostWebGLContext::Present)
DEFINE_ASYNC(HostWebGLContext::SampleCoverage)
DEFINE_ASYNC(HostWebGLContext::Scissor)
DEFINE_ASYNC(HostWebGLContext::ShaderSource)
DEFINE_ASYNC(HostWebGLContext::StencilFuncSeparate)
DEFINE_ASYNC(HostWebGLContext::StencilMaskSeparate)
DEFINE_ASYNC(HostWebGLContext::StencilOpSeparate)
DEFINE_ASYNC(HostWebGLContext::Viewport)
DEFINE_ASYNC(HostWebGLContext::BindBuffer)
DEFINE_ASYNC(HostWebGLContext::BindBufferRange)
DEFINE_ASYNC(HostWebGLContext::CopyBufferSubData)
DEFINE_ASYNC(HostWebGLContext::BufferData)
DEFINE_ASYNC(HostWebGLContext::BufferSubData)
DEFINE_ASYNC(HostWebGLContext::BlitFramebuffer)
DEFINE_ASYNC(HostWebGLContext::InvalidateFramebuffer)
DEFINE_ASYNC(HostWebGLContext::InvalidateSubFramebuffer)
DEFINE_ASYNC(HostWebGLContext::ReadBuffer)
DEFINE_ASYNC(HostWebGLContext::RenderbufferStorageMultisample)
DEFINE_ASYNC(HostWebGLContext::ActiveTexture)
DEFINE_ASYNC(HostWebGLContext::BindTexture)
DEFINE_ASYNC(HostWebGLContext::GenerateMipmap)
DEFINE_ASYNC(HostWebGLContext::CopyTexImage)
DEFINE_ASYNC(HostWebGLContext::TexStorage)
DEFINE_ASYNC(HostWebGLContext::TexImage)
DEFINE_ASYNC(HostWebGLContext::CompressedTexImage)
DEFINE_ASYNC(HostWebGLContext::TexParameter_base)
DEFINE_ASYNC(HostWebGLContext::UseProgram)
DEFINE_ASYNC(HostWebGLContext::UniformData)
DEFINE_ASYNC(HostWebGLContext::VertexAttrib4T)
DEFINE_ASYNC(HostWebGLContext::VertexAttribDivisor)
DEFINE_ASYNC(HostWebGLContext::UniformBlockBinding)
DEFINE_ASYNC(HostWebGLContext::EnableVertexAttribArray)
DEFINE_ASYNC(HostWebGLContext::DisableVertexAttribArray)
DEFINE_ASYNC(HostWebGLContext::VertexAttribPointer)
DEFINE_ASYNC(HostWebGLContext::ClearBufferTv)
DEFINE_ASYNC(HostWebGLContext::ClearBufferfi)
DEFINE_ASYNC(HostWebGLContext::ReadPixelsPbo)
DEFINE_ASYNC(HostWebGLContext::BindSampler)
DEFINE_ASYNC(HostWebGLContext::SamplerParameteri)
DEFINE_ASYNC(HostWebGLContext::SamplerParameterf)
DEFINE_ASYNC(HostWebGLContext::BindTransformFeedback)
DEFINE_ASYNC(HostWebGLContext::BeginTransformFeedback)
DEFINE_ASYNC(HostWebGLContext::EndTransformFeedback)
DEFINE_ASYNC(HostWebGLContext::PauseTransformFeedback)
DEFINE_ASYNC(HostWebGLContext::ResumeTransformFeedback)
DEFINE_ASYNC(HostWebGLContext::TransformFeedbackVaryings)
DEFINE_ASYNC(HostWebGLContext::DrawBuffers)
DEFINE_ASYNC(HostWebGLContext::BindVertexArray)
DEFINE_ASYNC(HostWebGLContext::DrawArraysInstanced)
DEFINE_ASYNC(HostWebGLContext::DrawElementsInstanced)
DEFINE_ASYNC(HostWebGLContext::BeginQuery)
DEFINE_ASYNC(HostWebGLContext::EndQuery)
DEFINE_ASYNC(HostWebGLContext::QueryCounter)
DEFINE_ASYNC(HostWebGLContext::SetFramebufferIsInOpaqueRAF)
DEFINE_ASYNC(HostWebGLContext::ClearVRSwapChain)

#undef DEFINE_ASYNC
#undef DEFINE_METHOD_DISPATCHER

}  // namespace mozilla

#endif  // WEBGLMETHODDISPATCHER_H_
