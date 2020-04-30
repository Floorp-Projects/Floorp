/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLMETHODDISPATCHER_H_
#define WEBGLMETHODDISPATCHER_H_

#include "TexUnpackBlob.h"
#include "WebGLCrossProcessCommandQueue.h"
#include "HostWebGLContext.h"
#include "WebGLQueueParamTraits.h"

namespace mozilla {

// The WebGLMethodDispatcher will dispatch commands read from the
// HostWebGLCommandSink and issue them to a given HostWebGLContext.
template <size_t id = 0>
class WebGLMethodDispatcher
    : public EmptyMethodDispatcher<WebGLMethodDispatcher> {};

#define DEFINE_METHOD_DISPATCHER(_ID, _METHOD, _SYNC)       \
  template <>                                               \
  class WebGLMethodDispatcher<_ID>                          \
      : public MethodDispatcher<WebGLMethodDispatcher, _ID, \
                                decltype(&_METHOD), &_METHOD, _SYNC> {};

// Defines each method the WebGLMethodDispatcher handles.  The COUNTER value
// is used as a cross-process ID for each of the methods.
#define DEFINE_METHOD_HELPER(_METHOD, _SYNC) \
  DEFINE_METHOD_DISPATCHER(__COUNTER__, _METHOD, _SYNC)
#define DEFINE_ASYNC(_METHOD) \
  DEFINE_METHOD_HELPER(_METHOD, CommandSyncType::ASYNC)
#define DEFINE_SYNC(_METHOD) \
  DEFINE_METHOD_HELPER(_METHOD, CommandSyncType::SYNC)

DEFINE_ASYNC(HostWebGLContext::CreateBuffer)
DEFINE_ASYNC(HostWebGLContext::CreateFramebuffer)
DEFINE_SYNC(HostWebGLContext::CreateOpaqueFramebuffer)
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

DEFINE_ASYNC(HostWebGLContext::ClearVRFrame)
// DEFINE_SYNC(HostWebGLContext::GetVRFrame)

DEFINE_ASYNC(HostWebGLContext::Disable)
DEFINE_ASYNC(HostWebGLContext::Enable)
DEFINE_ASYNC(HostWebGLContext::GenerateError)
DEFINE_SYNC(HostWebGLContext::GetCompileResult)
DEFINE_SYNC(HostWebGLContext::GetFragDataLocation)
// DEFINE_SYNC(HostWebGLContext::GetLinkResult)
DEFINE_SYNC(HostWebGLContext::InitializeCanvasRenderer)
DEFINE_SYNC(HostWebGLContext::IsEnabled)
DEFINE_ASYNC(HostWebGLContext::Resize)
DEFINE_ASYNC(HostWebGLContext::RequestExtension)
DEFINE_SYNC(HostWebGLContext::DrawingBufferSize)
DEFINE_SYNC(HostWebGLContext::OnMemoryPressure)
DEFINE_SYNC(HostWebGLContext::Present)
DEFINE_ASYNC(HostWebGLContext::DidRefresh)
DEFINE_SYNC(HostWebGLContext::GetParameter)
DEFINE_SYNC(HostWebGLContext::GetString)
DEFINE_ASYNC(HostWebGLContext::AttachShader)
DEFINE_ASYNC(HostWebGLContext::BindAttribLocation)
DEFINE_ASYNC(HostWebGLContext::BindFramebuffer)
DEFINE_ASYNC(HostWebGLContext::BlendColor)
DEFINE_ASYNC(HostWebGLContext::BlendEquationSeparate)
DEFINE_ASYNC(HostWebGLContext::BlendFuncSeparate)
DEFINE_SYNC(HostWebGLContext::CheckFramebufferStatus)
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
DEFINE_SYNC(HostWebGLContext::Finish)
DEFINE_ASYNC(HostWebGLContext::FramebufferAttach)
DEFINE_ASYNC(HostWebGLContext::FrontFace)
DEFINE_SYNC(HostWebGLContext::GetBufferParameter)
DEFINE_SYNC(HostWebGLContext::GetError)
DEFINE_SYNC(HostWebGLContext::GetFramebufferAttachmentParameter)
DEFINE_SYNC(HostWebGLContext::GetRenderbufferParameter)
DEFINE_SYNC(HostWebGLContext::GetShaderPrecisionFormat)
DEFINE_SYNC(HostWebGLContext::GetUniform)
DEFINE_ASYNC(HostWebGLContext::Hint)
DEFINE_ASYNC(HostWebGLContext::LineWidth)
DEFINE_ASYNC(HostWebGLContext::LinkProgram)
DEFINE_SYNC(HostWebGLContext::PixelStorei)
DEFINE_ASYNC(HostWebGLContext::PolygonOffset)
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
// DEFINE_ASYNC(HostWebGLContext::GetBufferSubData)
DEFINE_ASYNC(HostWebGLContext::BufferData)
DEFINE_ASYNC(HostWebGLContext::BufferSubData)
DEFINE_ASYNC(HostWebGLContext::BlitFramebuffer)
DEFINE_ASYNC(HostWebGLContext::InvalidateFramebuffer)
DEFINE_ASYNC(HostWebGLContext::InvalidateSubFramebuffer)
DEFINE_ASYNC(HostWebGLContext::ReadBuffer)
// DEFINE_SYNC(HostWebGLContext::GetInternalformatParameter)
DEFINE_ASYNC(HostWebGLContext::RenderbufferStorageMultisample)
DEFINE_ASYNC(HostWebGLContext::ActiveTexture)
DEFINE_ASYNC(HostWebGLContext::BindTexture)
DEFINE_ASYNC(HostWebGLContext::GenerateMipmap)
DEFINE_ASYNC(HostWebGLContext::CopyTexImage)
DEFINE_ASYNC(HostWebGLContext::TexStorage)
// DEFINE_ASYNC(HostWebGLContext::TexImage)
DEFINE_ASYNC(HostWebGLContext::CompressedTexImage)
DEFINE_SYNC(HostWebGLContext::GetTexParameter)
DEFINE_ASYNC(HostWebGLContext::TexParameter_base)
DEFINE_ASYNC(HostWebGLContext::UseProgram)
DEFINE_SYNC(HostWebGLContext::ValidateProgram)
DEFINE_ASYNC(HostWebGLContext::UniformData)
DEFINE_ASYNC(HostWebGLContext::VertexAttrib4T)
DEFINE_ASYNC(HostWebGLContext::VertexAttribDivisor)
DEFINE_SYNC(HostWebGLContext::GetIndexedParameter)
DEFINE_ASYNC(HostWebGLContext::UniformBlockBinding)
DEFINE_ASYNC(HostWebGLContext::EnableVertexAttribArray)
DEFINE_ASYNC(HostWebGLContext::DisableVertexAttribArray)
DEFINE_SYNC(HostWebGLContext::GetVertexAttrib)
DEFINE_ASYNC(HostWebGLContext::VertexAttribPointer)
DEFINE_ASYNC(HostWebGLContext::ClearBufferTv)
DEFINE_ASYNC(HostWebGLContext::ClearBufferfi)
// DEFINE_SYNC(HostWebGLContext::ReadPixels)
DEFINE_ASYNC(HostWebGLContext::ReadPixelsPbo)
DEFINE_ASYNC(HostWebGLContext::BindSampler)
DEFINE_ASYNC(HostWebGLContext::SamplerParameteri)
DEFINE_ASYNC(HostWebGLContext::SamplerParameterf)
DEFINE_SYNC(HostWebGLContext::GetSamplerParameter)
DEFINE_SYNC(HostWebGLContext::ClientWaitSync)
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
DEFINE_SYNC(HostWebGLContext::GetQueryParameter)
DEFINE_ASYNC(HostWebGLContext::SetFramebufferIsInOpaqueRAF)

#undef DEFINE_METHOD_HELPER
#undef DEFINE_ASYNC
#undef DEFINE_SYNC
#undef DEFINE_METHOD_DISPATCHER

}  // namespace mozilla

#endif  // WEBGLMETHODDISPATCHER_H_
