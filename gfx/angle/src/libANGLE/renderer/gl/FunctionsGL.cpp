//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// FunctionsGL.cpp: Implements the FuntionsGL class to contain loaded GL functions

#include "libANGLE/renderer/gl/FunctionsGL.h"

#include <algorithm>

#include "common/string_utils.h"
#include "libANGLE/renderer/gl/renderergl_utils.h"

namespace rx
{

static void GetGLVersion(PFNGLGETSTRINGPROC getStringFunction, gl::Version *outVersion, StandardGL *outStandard)
{
    const std::string version = reinterpret_cast<const char*>(getStringFunction(GL_VERSION));
    if (version.find("OpenGL ES") == std::string::npos)
    {
        // OpenGL spec states the GL_VERSION string will be in the following format:
        // <version number><space><vendor-specific information>
        // The version number is either of the form major number.minor number or major
        // number.minor number.release number, where the numbers all have one or more
        // digits
        *outStandard = STANDARD_GL_DESKTOP;
        *outVersion = gl::Version(version[0] - '0', version[2] - '0');
    }
    else
    {
        // ES spec states that the GL_VERSION string will be in the following format:
        // "OpenGL ES N.M vendor-specific information"
        *outStandard = STANDARD_GL_ES;
        *outVersion = gl::Version(version[10] - '0', version[12] - '0');
    }
}

static std::vector<std::string> GetIndexedExtensions(PFNGLGETINTEGERVPROC getIntegerFunction, PFNGLGETSTRINGIPROC getStringIFunction)
{
    std::vector<std::string> result;

    GLint numExtensions;
    getIntegerFunction(GL_NUM_EXTENSIONS, &numExtensions);

    result.reserve(numExtensions);

    for (GLint i = 0; i < numExtensions; i++)
    {
        result.push_back(reinterpret_cast<const char*>(getStringIFunction(GL_EXTENSIONS, i)));
    }

    return result;
}

static void AssignGLExtensionEntryPoint(const std::vector<std::string> &extensions,
                                        const char *requiredExtensionString,
                                        void *function,
                                        void **outFunction)
{
    std::vector<std::string> requiredExtensions;
    angle::SplitStringAlongWhitespace(requiredExtensionString, &requiredExtensions);
    for (const std::string& requiredExtension : requiredExtensions)
    {
        if (std::find(extensions.begin(), extensions.end(), requiredExtension) == extensions.end())
        {
            return;
        }
    }

    *outFunction = function;
}

#define ASSIGN_WITH_EXT(EXT, NAME, FP)                                  \
    AssignGLExtensionEntryPoint(extensions, EXT, loadProcAddress(NAME), \
                                reinterpret_cast<void **>(&FP))
#define ASSIGN(NAME, FP) *reinterpret_cast<void **>(&FP) = loadProcAddress(NAME)

FunctionsGL::FunctionsGL()
    : version(),
      standard(),
      extensions(),

      blendFunc(nullptr),
      clear(nullptr),
      clearColor(nullptr),
      clearDepth(nullptr),
      clearStencil(nullptr),
      colorMask(nullptr),
      cullFace(nullptr),
      depthFunc(nullptr),
      depthMask(nullptr),
      depthRange(nullptr),
      disable(nullptr),
      drawBuffer(nullptr),
      enable(nullptr),
      finish(nullptr),
      flush(nullptr),
      frontFace(nullptr),
      getBooleanv(nullptr),
      getDoublev(nullptr),
      getError(nullptr),
      getFloatv(nullptr),
      getIntegerv(nullptr),
      getString(nullptr),
      getTexImage(nullptr),
      getTexLevelParameterfv(nullptr),
      getTexLevelParameteriv(nullptr),
      getTexParameterfv(nullptr),
      getTexParameteriv(nullptr),
      hint(nullptr),
      isEnabled(nullptr),
      lineWidth(nullptr),
      logicOp(nullptr),
      pixelStoref(nullptr),
      pixelStorei(nullptr),
      pointSize(nullptr),
      polygonMode(nullptr),
      readBuffer(nullptr),
      readPixels(nullptr),
      scissor(nullptr),
      stencilFunc(nullptr),
      stencilMask(nullptr),
      stencilOp(nullptr),
      texImage1D(nullptr),
      texImage2D(nullptr),
      texParameterf(nullptr),
      texParameterfv(nullptr),
      texParameteri(nullptr),
      texParameteriv(nullptr),
      viewport(nullptr),

      bindTexture(nullptr),
      copyTexImage1D(nullptr),
      copyTexImage2D(nullptr),
      copyTexSubImage1D(nullptr),
      copyTexSubImage2D(nullptr),
      deleteTextures(nullptr),
      drawArrays(nullptr),
      drawElements(nullptr),
      genTextures(nullptr),
      isTexture(nullptr),
      polygonOffset(nullptr),
      texSubImage1D(nullptr),
      texSubImage2D(nullptr),

      blendColor(nullptr),
      blendEquation(nullptr),
      copyTexSubImage3D(nullptr),
      drawRangeElements(nullptr),
      texImage3D(nullptr),
      texSubImage3D(nullptr),

      deleteFencesNV(nullptr),
      genFencesNV(nullptr),
      isFenceNV(nullptr),
      testFenceNV(nullptr),
      getFenceivNV(nullptr),
      finishFenceNV(nullptr),
      setFenceNV(nullptr),

      activeTexture(nullptr),
      compressedTexImage1D(nullptr),
      compressedTexImage2D(nullptr),
      compressedTexImage3D(nullptr),
      compressedTexSubImage1D(nullptr),
      compressedTexSubImage2D(nullptr),
      compressedTexSubImage3D(nullptr),
      getCompressedTexImage(nullptr),
      sampleCoverage(nullptr),

      blendFuncSeparate(nullptr),
      multiDrawArrays(nullptr),
      multiDrawElements(nullptr),
      pointParameterf(nullptr),
      pointParameterfv(nullptr),
      pointParameteri(nullptr),
      pointParameteriv(nullptr),

      beginQuery(nullptr),
      bindBuffer(nullptr),
      bufferData(nullptr),
      bufferSubData(nullptr),
      deleteBuffers(nullptr),
      deleteQueries(nullptr),
      endQuery(nullptr),
      genBuffers(nullptr),
      genQueries(nullptr),
      getBufferParameteriv(nullptr),
      getBufferPointerv(nullptr),
      getBufferSubData(nullptr),
      getQueryObjectiv(nullptr),
      getQueryObjectuiv(nullptr),
      getQueryiv(nullptr),
      isBuffer(nullptr),
      isQuery(nullptr),
      mapBuffer(nullptr),
      unmapBuffer(nullptr),

      attachShader(nullptr),
      bindAttribLocation(nullptr),
      blendEquationSeparate(nullptr),
      compileShader(nullptr),
      createProgram(nullptr),
      createShader(nullptr),
      deleteProgram(nullptr),
      deleteShader(nullptr),
      detachShader(nullptr),
      disableVertexAttribArray(nullptr),
      drawBuffers(nullptr),
      enableVertexAttribArray(nullptr),
      getActiveAttrib(nullptr),
      getActiveUniform(nullptr),
      getAttachedShaders(nullptr),
      getAttribLocation(nullptr),
      getProgramInfoLog(nullptr),
      getProgramiv(nullptr),
      getShaderInfoLog(nullptr),
      getShaderSource(nullptr),
      getShaderiv(nullptr),
      getUniformLocation(nullptr),
      getUniformfv(nullptr),
      getUniformiv(nullptr),
      getVertexAttribPointerv(nullptr),
      getVertexAttribdv(nullptr),
      getVertexAttribfv(nullptr),
      getVertexAttribiv(nullptr),
      isProgram(nullptr),
      isShader(nullptr),
      linkProgram(nullptr),
      shaderSource(nullptr),
      stencilFuncSeparate(nullptr),
      stencilMaskSeparate(nullptr),
      stencilOpSeparate(nullptr),
      uniform1f(nullptr),
      uniform1fv(nullptr),
      uniform1i(nullptr),
      uniform1iv(nullptr),
      uniform2f(nullptr),
      uniform2fv(nullptr),
      uniform2i(nullptr),
      uniform2iv(nullptr),
      uniform3f(nullptr),
      uniform3fv(nullptr),
      uniform3i(nullptr),
      uniform3iv(nullptr),
      uniform4f(nullptr),
      uniform4fv(nullptr),
      uniform4i(nullptr),
      uniform4iv(nullptr),
      uniformMatrix2fv(nullptr),
      uniformMatrix3fv(nullptr),
      uniformMatrix4fv(nullptr),
      useProgram(nullptr),
      validateProgram(nullptr),
      vertexAttrib1d(nullptr),
      vertexAttrib1dv(nullptr),
      vertexAttrib1f(nullptr),
      vertexAttrib1fv(nullptr),
      vertexAttrib1s(nullptr),
      vertexAttrib1sv(nullptr),
      vertexAttrib2d(nullptr),
      vertexAttrib2dv(nullptr),
      vertexAttrib2f(nullptr),
      vertexAttrib2fv(nullptr),
      vertexAttrib2s(nullptr),
      vertexAttrib2sv(nullptr),
      vertexAttrib3d(nullptr),
      vertexAttrib3dv(nullptr),
      vertexAttrib3f(nullptr),
      vertexAttrib3fv(nullptr),
      vertexAttrib3s(nullptr),
      vertexAttrib3sv(nullptr),
      vertexAttrib4Nbv(nullptr),
      vertexAttrib4Niv(nullptr),
      vertexAttrib4Nsv(nullptr),
      vertexAttrib4Nub(nullptr),
      vertexAttrib4Nubv(nullptr),
      vertexAttrib4Nuiv(nullptr),
      vertexAttrib4Nusv(nullptr),
      vertexAttrib4bv(nullptr),
      vertexAttrib4d(nullptr),
      vertexAttrib4dv(nullptr),
      vertexAttrib4f(nullptr),
      vertexAttrib4fv(nullptr),
      vertexAttrib4iv(nullptr),
      vertexAttrib4s(nullptr),
      vertexAttrib4sv(nullptr),
      vertexAttrib4ubv(nullptr),
      vertexAttrib4uiv(nullptr),
      vertexAttrib4usv(nullptr),
      vertexAttribPointer(nullptr),

      uniformMatrix2x3fv(nullptr),
      uniformMatrix2x4fv(nullptr),
      uniformMatrix3x2fv(nullptr),
      uniformMatrix3x4fv(nullptr),
      uniformMatrix4x2fv(nullptr),
      uniformMatrix4x3fv(nullptr),

      beginConditionalRender(nullptr),
      beginTransformFeedback(nullptr),
      bindBufferBase(nullptr),
      bindBufferRange(nullptr),
      bindFragDataLocation(nullptr),
      bindFramebuffer(nullptr),
      bindRenderbuffer(nullptr),
      bindVertexArray(nullptr),
      blitFramebuffer(nullptr),
      checkFramebufferStatus(nullptr),
      clampColor(nullptr),
      clearBufferfi(nullptr),
      clearBufferfv(nullptr),
      clearBufferiv(nullptr),
      clearBufferuiv(nullptr),
      colorMaski(nullptr),
      deleteFramebuffers(nullptr),
      deleteRenderbuffers(nullptr),
      deleteVertexArrays(nullptr),
      disablei(nullptr),
      enablei(nullptr),
      endConditionalRender(nullptr),
      endTransformFeedback(nullptr),
      flushMappedBufferRange(nullptr),
      framebufferRenderbuffer(nullptr),
      framebufferTexture1D(nullptr),
      framebufferTexture2D(nullptr),
      framebufferTexture3D(nullptr),
      framebufferTextureLayer(nullptr),
      genFramebuffers(nullptr),
      genRenderbuffers(nullptr),
      genVertexArrays(nullptr),
      generateMipmap(nullptr),
      getBooleani_v(nullptr),
      getFragDataLocation(nullptr),
      getFramebufferAttachmentParameteriv(nullptr),
      getIntegeri_v(nullptr),
      getRenderbufferParameteriv(nullptr),
      getStringi(nullptr),
      getTexParameterIiv(nullptr),
      getTexParameterIuiv(nullptr),
      getTransformFeedbackVarying(nullptr),
      getUniformuiv(nullptr),
      getVertexAttribIiv(nullptr),
      getVertexAttribIuiv(nullptr),
      isEnabledi(nullptr),
      isFramebuffer(nullptr),
      isRenderbuffer(nullptr),
      isVertexArray(nullptr),
      mapBufferRange(nullptr),
      renderbufferStorage(nullptr),
      renderbufferStorageMultisample(nullptr),
      texParameterIiv(nullptr),
      texParameterIuiv(nullptr),
      transformFeedbackVaryings(nullptr),
      uniform1ui(nullptr),
      uniform1uiv(nullptr),
      uniform2ui(nullptr),
      uniform2uiv(nullptr),
      uniform3ui(nullptr),
      uniform3uiv(nullptr),
      uniform4ui(nullptr),
      uniform4uiv(nullptr),
      vertexAttribI1i(nullptr),
      vertexAttribI1iv(nullptr),
      vertexAttribI1ui(nullptr),
      vertexAttribI1uiv(nullptr),
      vertexAttribI2i(nullptr),
      vertexAttribI2iv(nullptr),
      vertexAttribI2ui(nullptr),
      vertexAttribI2uiv(nullptr),
      vertexAttribI3i(nullptr),
      vertexAttribI3iv(nullptr),
      vertexAttribI3ui(nullptr),
      vertexAttribI3uiv(nullptr),
      vertexAttribI4bv(nullptr),
      vertexAttribI4i(nullptr),
      vertexAttribI4iv(nullptr),
      vertexAttribI4sv(nullptr),
      vertexAttribI4ubv(nullptr),
      vertexAttribI4ui(nullptr),
      vertexAttribI4uiv(nullptr),
      vertexAttribI4usv(nullptr),
      vertexAttribIPointer(nullptr),

      copyBufferSubData(nullptr),
      drawArraysInstanced(nullptr),
      drawElementsInstanced(nullptr),
      getActiveUniformBlockName(nullptr),
      getActiveUniformBlockiv(nullptr),
      getActiveUniformName(nullptr),
      getActiveUniformsiv(nullptr),
      getUniformBlockIndex(nullptr),
      getUniformIndices(nullptr),
      primitiveRestartIndex(nullptr),
      texBuffer(nullptr),
      uniformBlockBinding(nullptr),

      clientWaitSync(nullptr),
      deleteSync(nullptr),
      drawElementsBaseVertex(nullptr),
      drawElementsInstancedBaseVertex(nullptr),
      drawRangeElementsBaseVertex(nullptr),
      fenceSync(nullptr),
      framebufferTexture(nullptr),
      getBufferParameteri64v(nullptr),
      getInteger64i_v(nullptr),
      getInteger64v(nullptr),
      getMultisamplefv(nullptr),
      getSynciv(nullptr),
      isSync(nullptr),
      multiDrawElementsBaseVertex(nullptr),
      provokingVertex(nullptr),
      sampleMaski(nullptr),
      texImage2DMultisample(nullptr),
      texImage3DMultisample(nullptr),
      waitSync(nullptr),

      matrixLoadEXT(nullptr),
      genPathsNV(nullptr),
      delPathsNV(nullptr),
      pathCommandsNV(nullptr),
      setPathParameterfNV(nullptr),
      setPathParameteriNV(nullptr),
      getPathParameterfNV(nullptr),
      getPathParameteriNV(nullptr),
      pathStencilFuncNV(nullptr),
      stencilFillPathNV(nullptr),
      stencilStrokePathNV(nullptr),
      coverFillPathNV(nullptr),
      coverStrokePathNV(nullptr),
      stencilThenCoverFillPathNV(nullptr),
      stencilThenCoverStrokePathNV(nullptr),
      coverFillPathInstancedNV(nullptr),
      coverStrokePathInstancedNV(nullptr),
      stencilFillPathInstancedNV(nullptr),
      stencilStrokePathInstancedNV(nullptr),
      stencilThenCoverFillPathInstancedNV(nullptr),
      stencilThenCoverStrokePathInstancedNV(nullptr),
      programPathFragmentInputGenNV(nullptr),

      bindFragDataLocationIndexed(nullptr),
      bindSampler(nullptr),
      deleteSamplers(nullptr),
      genSamplers(nullptr),
      getFragDataIndex(nullptr),
      getQueryObjecti64v(nullptr),
      getQueryObjectui64v(nullptr),
      getSamplerParameterIiv(nullptr),
      getSamplerParameterIuiv(nullptr),
      getSamplerParameterfv(nullptr),
      getSamplerParameteriv(nullptr),
      isSampler(nullptr),
      queryCounter(nullptr),
      samplerParameterIiv(nullptr),
      samplerParameterIuiv(nullptr),
      samplerParameterf(nullptr),
      samplerParameterfv(nullptr),
      samplerParameteri(nullptr),
      samplerParameteriv(nullptr),
      vertexAttribDivisor(nullptr),
      vertexAttribP1ui(nullptr),
      vertexAttribP1uiv(nullptr),
      vertexAttribP2ui(nullptr),
      vertexAttribP2uiv(nullptr),
      vertexAttribP3ui(nullptr),
      vertexAttribP3uiv(nullptr),
      vertexAttribP4ui(nullptr),
      vertexAttribP4uiv(nullptr),

      beginQueryIndexed(nullptr),
      bindTransformFeedback(nullptr),
      blendEquationSeparatei(nullptr),
      blendEquationi(nullptr),
      blendFuncSeparatei(nullptr),
      blendFunci(nullptr),
      deleteTransformFeedbacks(nullptr),
      drawArraysIndirect(nullptr),
      drawElementsIndirect(nullptr),
      drawTransformFeedback(nullptr),
      drawTransformFeedbackStream(nullptr),
      endQueryIndexed(nullptr),
      genTransformFeedbacks(nullptr),
      getActiveSubroutineName(nullptr),
      getActiveSubroutineUniformName(nullptr),
      getActiveSubroutineUniformiv(nullptr),
      getProgramStageiv(nullptr),
      getQueryIndexediv(nullptr),
      getSubroutineIndex(nullptr),
      getSubroutineUniformLocation(nullptr),
      getUniformSubroutineuiv(nullptr),
      getUniformdv(nullptr),
      isTransformFeedback(nullptr),
      minSampleShading(nullptr),
      patchParameterfv(nullptr),
      patchParameteri(nullptr),
      pauseTransformFeedback(nullptr),
      resumeTransformFeedback(nullptr),
      uniform1d(nullptr),
      uniform1dv(nullptr),
      uniform2d(nullptr),
      uniform2dv(nullptr),
      uniform3d(nullptr),
      uniform3dv(nullptr),
      uniform4d(nullptr),
      uniform4dv(nullptr),
      uniformMatrix2dv(nullptr),
      uniformMatrix2x3dv(nullptr),
      uniformMatrix2x4dv(nullptr),
      uniformMatrix3dv(nullptr),
      uniformMatrix3x2dv(nullptr),
      uniformMatrix3x4dv(nullptr),
      uniformMatrix4dv(nullptr),
      uniformMatrix4x2dv(nullptr),
      uniformMatrix4x3dv(nullptr),
      uniformSubroutinesuiv(nullptr),

      activeShaderProgram(nullptr),
      bindProgramPipeline(nullptr),
      clearDepthf(nullptr),
      createShaderProgramv(nullptr),
      deleteProgramPipelines(nullptr),
      depthRangeArrayv(nullptr),
      depthRangeIndexed(nullptr),
      depthRangef(nullptr),
      genProgramPipelines(nullptr),
      getDoublei_v(nullptr),
      getFloati_v(nullptr),
      getProgramBinary(nullptr),
      getProgramPipelineInfoLog(nullptr),
      getProgramPipelineiv(nullptr),
      getShaderPrecisionFormat(nullptr),
      getVertexAttribLdv(nullptr),
      isProgramPipeline(nullptr),
      programBinary(nullptr),
      programParameteri(nullptr),
      programUniform1d(nullptr),
      programUniform1dv(nullptr),
      programUniform1f(nullptr),
      programUniform1fv(nullptr),
      programUniform1i(nullptr),
      programUniform1iv(nullptr),
      programUniform1ui(nullptr),
      programUniform1uiv(nullptr),
      programUniform2d(nullptr),
      programUniform2dv(nullptr),
      programUniform2f(nullptr),
      programUniform2fv(nullptr),
      programUniform2i(nullptr),
      programUniform2iv(nullptr),
      programUniform2ui(nullptr),
      programUniform2uiv(nullptr),
      programUniform3d(nullptr),
      programUniform3dv(nullptr),
      programUniform3f(nullptr),
      programUniform3fv(nullptr),
      programUniform3i(nullptr),
      programUniform3iv(nullptr),
      programUniform3ui(nullptr),
      programUniform3uiv(nullptr),
      programUniform4d(nullptr),
      programUniform4dv(nullptr),
      programUniform4f(nullptr),
      programUniform4fv(nullptr),
      programUniform4i(nullptr),
      programUniform4iv(nullptr),
      programUniform4ui(nullptr),
      programUniform4uiv(nullptr),
      programUniformMatrix2dv(nullptr),
      programUniformMatrix2fv(nullptr),
      programUniformMatrix2x3dv(nullptr),
      programUniformMatrix2x3fv(nullptr),
      programUniformMatrix2x4dv(nullptr),
      programUniformMatrix2x4fv(nullptr),
      programUniformMatrix3dv(nullptr),
      programUniformMatrix3fv(nullptr),
      programUniformMatrix3x2dv(nullptr),
      programUniformMatrix3x2fv(nullptr),
      programUniformMatrix3x4dv(nullptr),
      programUniformMatrix3x4fv(nullptr),
      programUniformMatrix4dv(nullptr),
      programUniformMatrix4fv(nullptr),
      programUniformMatrix4x2dv(nullptr),
      programUniformMatrix4x2fv(nullptr),
      programUniformMatrix4x3dv(nullptr),
      programUniformMatrix4x3fv(nullptr),
      releaseShaderCompiler(nullptr),
      scissorArrayv(nullptr),
      scissorIndexed(nullptr),
      scissorIndexedv(nullptr),
      shaderBinary(nullptr),
      useProgramStages(nullptr),
      validateProgramPipeline(nullptr),
      vertexAttribL1d(nullptr),
      vertexAttribL1dv(nullptr),
      vertexAttribL2d(nullptr),
      vertexAttribL2dv(nullptr),
      vertexAttribL3d(nullptr),
      vertexAttribL3dv(nullptr),
      vertexAttribL4d(nullptr),
      vertexAttribL4dv(nullptr),
      vertexAttribLPointer(nullptr),
      viewportArrayv(nullptr),
      viewportIndexedf(nullptr),
      viewportIndexedfv(nullptr),

      bindImageTexture(nullptr),
      drawArraysInstancedBaseInstance(nullptr),
      drawElementsInstancedBaseInstance(nullptr),
      drawElementsInstancedBaseVertexBaseInstance(nullptr),
      drawTransformFeedbackInstanced(nullptr),
      drawTransformFeedbackStreamInstanced(nullptr),
      getActiveAtomicCounterBufferiv(nullptr),
      getInternalformativ(nullptr),
      memoryBarrier(nullptr),
      texStorage1D(nullptr),
      texStorage2D(nullptr),
      texStorage3D(nullptr),

      bindVertexBuffer(nullptr),
      clearBufferData(nullptr),
      clearBufferSubData(nullptr),
      copyImageSubData(nullptr),
      debugMessageCallback(nullptr),
      debugMessageControl(nullptr),
      debugMessageInsert(nullptr),
      dispatchCompute(nullptr),
      dispatchComputeIndirect(nullptr),
      framebufferParameteri(nullptr),
      getDebugMessageLog(nullptr),
      getFramebufferParameteriv(nullptr),
      getInternalformati64v(nullptr),
      getPointerv(nullptr),
      getObjectLabel(nullptr),
      getObjectPtrLabel(nullptr),
      getProgramInterfaceiv(nullptr),
      getProgramResourceIndex(nullptr),
      getProgramResourceLocation(nullptr),
      getProgramResourceLocationIndex(nullptr),
      getProgramResourceName(nullptr),
      getProgramResourceiv(nullptr),
      invalidateBufferData(nullptr),
      invalidateBufferSubData(nullptr),
      invalidateFramebuffer(nullptr),
      invalidateSubFramebuffer(nullptr),
      invalidateTexImage(nullptr),
      invalidateTexSubImage(nullptr),
      multiDrawArraysIndirect(nullptr),
      multiDrawElementsIndirect(nullptr),
      objectLabel(nullptr),
      objectPtrLabel(nullptr),
      popDebugGroup(nullptr),
      pushDebugGroup(nullptr),
      shaderStorageBlockBinding(nullptr),
      texBufferRange(nullptr),
      texStorage2DMultisample(nullptr),
      texStorage3DMultisample(nullptr),
      textureView(nullptr),
      vertexAttribBinding(nullptr),
      vertexAttribFormat(nullptr),
      vertexAttribIFormat(nullptr),
      vertexAttribLFormat(nullptr),
      vertexBindingDivisor(nullptr),
      coverageModulationNV(nullptr),

      bindBuffersBase(nullptr),
      bindBuffersRange(nullptr),
      bindImageTextures(nullptr),
      bindSamplers(nullptr),
      bindTextures(nullptr),
      bindVertexBuffers(nullptr),
      bufferStorage(nullptr),
      clearTexImage(nullptr),
      clearTexSubImage(nullptr),

      bindTextureUnit(nullptr),
      blitNamedFramebuffer(nullptr),
      checkNamedFramebufferStatus(nullptr),
      clearNamedBufferData(nullptr),
      clearNamedBufferSubData(nullptr),
      clearNamedFramebufferfi(nullptr),
      clearNamedFramebufferfv(nullptr),
      clearNamedFramebufferiv(nullptr),
      clearNamedFramebufferuiv(nullptr),
      clipControl(nullptr),
      compressedTextureSubImage1D(nullptr),
      compressedTextureSubImage2D(nullptr),
      compressedTextureSubImage3D(nullptr),
      copyNamedBufferSubData(nullptr),
      copyTextureSubImage1D(nullptr),
      copyTextureSubImage2D(nullptr),
      copyTextureSubImage3D(nullptr),
      createBuffers(nullptr),
      createFramebuffers(nullptr),
      createProgramPipelines(nullptr),
      createQueries(nullptr),
      createRenderbuffers(nullptr),
      createSamplers(nullptr),
      createTextures(nullptr),
      createTransformFeedbacks(nullptr),
      createVertexArrays(nullptr),
      disableVertexArrayAttrib(nullptr),
      enableVertexArrayAttrib(nullptr),
      flushMappedNamedBufferRange(nullptr),
      generateTextureMipmap(nullptr),
      getCompressedTextureImage(nullptr),
      getCompressedTextureSubImage(nullptr),
      getGraphicsResetStatus(nullptr),
      getNamedBufferParameteri64v(nullptr),
      getNamedBufferParameteriv(nullptr),
      getNamedBufferPointerv(nullptr),
      getNamedBufferSubData(nullptr),
      getNamedFramebufferAttachmentParameteriv(nullptr),
      getNamedFramebufferParameteriv(nullptr),
      getNamedRenderbufferParameteriv(nullptr),
      getQueryBufferObjecti64v(nullptr),
      getQueryBufferObjectiv(nullptr),
      getQueryBufferObjectui64v(nullptr),
      getQueryBufferObjectuiv(nullptr),
      getTextureImage(nullptr),
      getTextureLevelParameterfv(nullptr),
      getTextureLevelParameteriv(nullptr),
      getTextureParameterIiv(nullptr),
      getTextureParameterIuiv(nullptr),
      getTextureParameterfv(nullptr),
      getTextureParameteriv(nullptr),
      getTextureSubImage(nullptr),
      getTransformFeedbacki64_v(nullptr),
      getTransformFeedbacki_v(nullptr),
      getTransformFeedbackiv(nullptr),
      getVertexArrayIndexed64iv(nullptr),
      getVertexArrayIndexediv(nullptr),
      getVertexArrayiv(nullptr),
      getnCompressedTexImage(nullptr),
      getnTexImage(nullptr),
      getnUniformdv(nullptr),
      getnUniformfv(nullptr),
      getnUniformiv(nullptr),
      getnUniformuiv(nullptr),
      invalidateNamedFramebufferData(nullptr),
      invalidateNamedFramebufferSubData(nullptr),
      mapNamedBuffer(nullptr),
      mapNamedBufferRange(nullptr),
      memoryBarrierByRegion(nullptr),
      namedBufferData(nullptr),
      namedBufferStorage(nullptr),
      namedBufferSubData(nullptr),
      namedFramebufferDrawBuffer(nullptr),
      namedFramebufferDrawBuffers(nullptr),
      namedFramebufferParameteri(nullptr),
      namedFramebufferReadBuffer(nullptr),
      namedFramebufferRenderbuffer(nullptr),
      namedFramebufferTexture(nullptr),
      namedFramebufferTextureLayer(nullptr),
      namedRenderbufferStorage(nullptr),
      namedRenderbufferStorageMultisample(nullptr),
      readnPixels(nullptr),
      textureBarrier(nullptr),
      textureBuffer(nullptr),
      textureBufferRange(nullptr),
      textureParameterIiv(nullptr),
      textureParameterIuiv(nullptr),
      textureParameterf(nullptr),
      textureParameterfv(nullptr),
      textureParameteri(nullptr),
      textureParameteriv(nullptr),
      textureStorage1D(nullptr),
      textureStorage2D(nullptr),
      textureStorage2DMultisample(nullptr),
      textureStorage3D(nullptr),
      textureStorage3DMultisample(nullptr),
      textureSubImage1D(nullptr),
      textureSubImage2D(nullptr),
      textureSubImage3D(nullptr),
      transformFeedbackBufferBase(nullptr),
      transformFeedbackBufferRange(nullptr),
      unmapNamedBuffer(nullptr),
      vertexArrayAttribBinding(nullptr),
      vertexArrayAttribFormat(nullptr),
      vertexArrayAttribIFormat(nullptr),
      vertexArrayAttribLFormat(nullptr),
      vertexArrayBindingDivisor(nullptr),
      vertexArrayElementBuffer(nullptr),
      vertexArrayVertexBuffer(nullptr),
      vertexArrayVertexBuffers(nullptr),
      blendBarrier(nullptr),
      primitiveBoundingBox(nullptr),
      eglImageTargetRenderbufferStorageOES(nullptr),
      eglImageTargetTexture2DOES(nullptr),
      discardFramebuffer(nullptr),
      getInternalformatSampleivNV(nullptr)
{
}

FunctionsGL::~FunctionsGL()
{
}

void FunctionsGL::initialize()
{
    // Grab the version number
    ASSIGN("glGetString", getString);
    ASSIGN("glGetIntegerv", getIntegerv);
    GetGLVersion(getString, &version, &standard);

    // Grab the GL extensions
    if (isAtLeastGL(gl::Version(3, 0)) || isAtLeastGLES(gl::Version(3, 0)))
    {
        ASSIGN("glGetStringi", getStringi);
        extensions = GetIndexedExtensions(getIntegerv, getStringi);
    }
    else
    {
        const char *exts = reinterpret_cast<const char*>(getString(GL_EXTENSIONS));
        angle::SplitStringAlongWhitespace(std::string(exts), &extensions);
    }

    // Load the entry points
    switch (standard)
    {
        case STANDARD_GL_DESKTOP:
            initializeProcsDesktopGL();
            break;

        case STANDARD_GL_ES:
            initializeProcsGLES();
            break;

        default:
            UNREACHABLE();
            break;
    }
}

void FunctionsGL::initializeProcsDesktopGL()
{
    // Check the context profile
    profile = 0;
    if (isAtLeastGL(gl::Version(3, 2)))
    {
        getIntegerv(GL_CONTEXT_PROFILE_MASK, &profile);
    }

    // clang-format off

    // Load extensions
    // Even though extensions are written against specific versions of GL, many drivers expose the extensions
    // in even older versions.  Always try loading the extensions regardless of GL version.

    // GL_NV_internalformat_sample_query
    ASSIGN_WITH_EXT("GL_NV_internalformat_sample_query", "glGetInternalformatSampleivNV", getInternalformatSampleivNV);

    // GL_ARB_program_interface_query (loading only functions relevant to GL_NV_path_rendering here)
    ASSIGN_WITH_EXT("GL_ARB_program_interface_query", "glGetProgramInterfaceiv", getProgramInterfaceiv);
    ASSIGN_WITH_EXT("GL_ARB_program_interface_query", "glGetProgramResourceName", getProgramResourceName);
    ASSIGN_WITH_EXT("GL_ARB_program_interface_query", "glGetProgramResourceiv", getProgramResourceiv);

    // GL_NV_path_rendering
    ASSIGN_WITH_EXT("GL_NV_path_rendering", "glMatrixLoadfEXT", matrixLoadEXT);
    ASSIGN_WITH_EXT("GL_NV_path_rendering", "glGenPathsNV", genPathsNV);
    ASSIGN_WITH_EXT("GL_NV_path_rendering", "glDeletePathsNV", delPathsNV);
    ASSIGN_WITH_EXT("GL_NV_path_rendering", "glPathCommandsNV", pathCommandsNV);
    ASSIGN_WITH_EXT("GL_NV_path_rendering", "glIsPathNV", isPathNV);
    ASSIGN_WITH_EXT("GL_NV_path_rendering", "glPathParameterfNV", setPathParameterfNV);
    ASSIGN_WITH_EXT("GL_NV_path_rendering", "glPathParameteriNV", setPathParameteriNV);
    ASSIGN_WITH_EXT("GL_NV_path_rendering", "glGetPathParameterfvNV", getPathParameterfNV);
    ASSIGN_WITH_EXT("GL_NV_path_rendering", "glGetPathParameterivNV", getPathParameteriNV);
    ASSIGN_WITH_EXT("GL_NV_path_rendering", "glPathStencilFuncNV", pathStencilFuncNV);
    ASSIGN_WITH_EXT("GL_NV_path_rendering", "glStencilFillPathNV", stencilFillPathNV);
    ASSIGN_WITH_EXT("GL_NV_path_rendering", "glStencilStrokePathNV", stencilStrokePathNV);
    ASSIGN_WITH_EXT("GL_NV_path_rendering", "glCoverFillPathNV", coverFillPathNV);
    ASSIGN_WITH_EXT("GL_NV_path_rendering", "glCoverStrokePathNV", coverStrokePathNV);
    ASSIGN_WITH_EXT("GL_NV_path_rendering", "glStencilThenCoverFillPathNV", stencilThenCoverFillPathNV);
    ASSIGN_WITH_EXT("GL_NV_path_rendering", "glStencilThenCoverStrokePathNV", stencilThenCoverStrokePathNV);
    ASSIGN_WITH_EXT("GL_NV_path_rendering", "glCoverFillPathInstancedNV", coverFillPathInstancedNV);
    ASSIGN_WITH_EXT("GL_NV_path_rendering", "glCoverStrokePathInstancedNV", coverStrokePathInstancedNV);
    ASSIGN_WITH_EXT("GL_NV_path_rendering", "glStencilFillPathInstancedNV", stencilFillPathInstancedNV);
    ASSIGN_WITH_EXT("GL_NV_path_rendering", "glStencilStrokePathInstancedNV", stencilStrokePathInstancedNV);
    ASSIGN_WITH_EXT("GL_NV_path_rendering", "glStencilThenCoverFillPathInstancedNV", stencilThenCoverFillPathInstancedNV);
    ASSIGN_WITH_EXT("GL_NV_path_rendering", "glStencilThenCoverStrokePathInstancedNV", stencilThenCoverStrokePathInstancedNV);
    ASSIGN_WITH_EXT("GL_NV_path_rendering", "glProgramPathFragmentInputGenNV", programPathFragmentInputGenNV);

    // GL_NV_framebuffer_mixed_samples
    ASSIGN_WITH_EXT("GL_NV_framebuffer_mixed_samples", "glCoverageModulationNV", coverageModulationNV);

    // GL_NV_fence
    ASSIGN_WITH_EXT("GL_NV_fence", "glDeleteFencesNV", deleteFencesNV);
    ASSIGN_WITH_EXT("GL_NV_fence", "glGenFencesNV", genFencesNV);
    ASSIGN_WITH_EXT("GL_NV_fence", "glIsFenceNV", isFenceNV);
    ASSIGN_WITH_EXT("GL_NV_fence", "glTestFenceNV", testFenceNV);
    ASSIGN_WITH_EXT("GL_NV_fence", "glGetFenceivNV", getFenceivNV);
    ASSIGN_WITH_EXT("GL_NV_fence", "glFinishFenceNV", finishFenceNV);
    ASSIGN_WITH_EXT("GL_NV_fence", "glSetFenceNV", setFenceNV);

    // GL_EXT_texture_storage
    ASSIGN_WITH_EXT("GL_EXT_texture_storage", "glTexStorage1DEXT", texStorage1D);
    ASSIGN_WITH_EXT("GL_EXT_texture_storage", "glTexStorage2DEXT", texStorage2D);
    ASSIGN_WITH_EXT("GL_EXT_texture_storage GL_EXT_texture3D", "glTexStorage3DEXT", texStorage3D);
    ASSIGN_WITH_EXT("GL_EXT_texture_storage GL_EXT_texture3D", "glTextureStorage1DEXT", textureStorage1D);
    ASSIGN_WITH_EXT("GL_EXT_texture_storage GL_EXT_direct_state_access", "glTextureStorage2DEXT", textureStorage2D);
    ASSIGN_WITH_EXT("GL_EXT_texture_storage GL_EXT_direct_state_access GL_EXT_texture3D", "glTextureStorage3DEXT", textureStorage3D);

    // GL_ARB_vertex_array_object
    ASSIGN_WITH_EXT("GL_ARB_vertex_array_object", "glBindVertexArray", bindVertexArray);
    ASSIGN_WITH_EXT("GL_ARB_vertex_array_object", "glDeleteVertexArrays", deleteVertexArrays);
    ASSIGN_WITH_EXT("GL_ARB_vertex_array_object", "glGenVertexArrays", genVertexArrays);
    ASSIGN_WITH_EXT("GL_ARB_vertex_array_object", "glIsVertexArray", isVertexArray);

    // GL_ARB_vertex_attrib_binding
    ASSIGN_WITH_EXT("GL_ARB_vertex_attrib_binding", "glBindVertexBuffer", bindVertexBuffer);
    ASSIGN_WITH_EXT("GL_ARB_vertex_attrib_binding", "glVertexAttribFormat", vertexAttribFormat);
    ASSIGN_WITH_EXT("GL_ARB_vertex_attrib_binding", "glVertexAttribIFormat", vertexAttribIFormat);
    ASSIGN_WITH_EXT("GL_ARB_vertex_attrib_binding", "glVertexAttribLFormat", vertexAttribLFormat);
    ASSIGN_WITH_EXT("GL_ARB_vertex_attrib_binding", "glVertexAttribBinding", vertexAttribBinding);
    ASSIGN_WITH_EXT("GL_ARB_vertex_attrib_binding", "glVertexBindingDivisor", vertexBindingDivisor);

    // GL_ARB_sync
    ASSIGN_WITH_EXT("GL_ARB_sync", "glClientWaitSync", clientWaitSync);
    ASSIGN_WITH_EXT("GL_ARB_sync", "glDeleteSync", deleteSync);
    ASSIGN_WITH_EXT("GL_ARB_sync", "glFenceSync", fenceSync);
    ASSIGN_WITH_EXT("GL_ARB_sync", "glGetInteger64i_v", getInteger64i_v);
    ASSIGN_WITH_EXT("GL_ARB_sync", "glGetInteger64v", getInteger64v);
    ASSIGN_WITH_EXT("GL_ARB_sync", "glGetSynciv", getSynciv);
    ASSIGN_WITH_EXT("GL_ARB_sync", "glIsSync", isSync);
    ASSIGN_WITH_EXT("GL_ARB_sync", "glWaitSync", waitSync);

    // GL_EXT_framebuffer_object
    ASSIGN_WITH_EXT("GL_EXT_framebuffer_object", "glIsRenderbufferEXT", isRenderbuffer);
    ASSIGN_WITH_EXT("GL_EXT_framebuffer_object", "glBindRenderbufferEXT", bindRenderbuffer);
    ASSIGN_WITH_EXT("GL_EXT_framebuffer_object", "glDeleteRenderbuffersEXT", deleteRenderbuffers);
    ASSIGN_WITH_EXT("GL_EXT_framebuffer_object", "glGenRenderbuffersEXT", genRenderbuffers);
    ASSIGN_WITH_EXT("GL_EXT_framebuffer_object", "glRenderbufferStorageEXT", renderbufferStorage);
    ASSIGN_WITH_EXT("GL_EXT_framebuffer_object", "glGetRenderbufferParameterivEXT", getRenderbufferParameteriv);
    ASSIGN_WITH_EXT("GL_EXT_framebuffer_object", "glIsFramebufferEXT", isFramebuffer);
    ASSIGN_WITH_EXT("GL_EXT_framebuffer_object", "glBindFramebufferEXT", bindFramebuffer);
    ASSIGN_WITH_EXT("GL_EXT_framebuffer_object", "glDeleteFramebuffersEXT", deleteFramebuffers);
    ASSIGN_WITH_EXT("GL_EXT_framebuffer_object", "glGenFramebuffersEXT", genFramebuffers);
    ASSIGN_WITH_EXT("GL_EXT_framebuffer_object", "glCheckFramebufferStatusEXT", checkFramebufferStatus);
    ASSIGN_WITH_EXT("GL_EXT_framebuffer_object", "glFramebufferTexture1DEXT", framebufferTexture1D);
    ASSIGN_WITH_EXT("GL_EXT_framebuffer_object", "glFramebufferTexture2DEXT", framebufferTexture2D);
    ASSIGN_WITH_EXT("GL_EXT_framebuffer_object", "glFramebufferTexture3DEXT", framebufferTexture3D);
    ASSIGN_WITH_EXT("GL_EXT_framebuffer_object", "glFramebufferRenderbufferEXT", framebufferRenderbuffer);
    ASSIGN_WITH_EXT("GL_EXT_framebuffer_object", "glGetFramebufferAttachmentParameterivEXT", getFramebufferAttachmentParameteriv);
    ASSIGN_WITH_EXT("GL_EXT_framebuffer_object", "glGenerateMipmapEXT", generateMipmap);

    // GL_EXT_framebuffer_blit
    ASSIGN_WITH_EXT("GL_EXT_framebuffer_blit", "glBlitFramebufferEXT", blitFramebuffer);

    // GL_KHR_debug
    ASSIGN_WITH_EXT("GL_KHR_debug", "glDebugMessageControl", debugMessageControl);
    ASSIGN_WITH_EXT("GL_KHR_debug", "glDebugMessageInsert", debugMessageInsert);
    ASSIGN_WITH_EXT("GL_KHR_debug", "glDebugMessageCallback", debugMessageCallback);
    ASSIGN_WITH_EXT("GL_KHR_debug", "glGetDebugMessageLog", getDebugMessageLog);
    ASSIGN_WITH_EXT("GL_KHR_debug", "glGetPointerv", getPointerv);
    ASSIGN_WITH_EXT("GL_KHR_debug", "glPushDebugGroup", pushDebugGroup);
    ASSIGN_WITH_EXT("GL_KHR_debug", "glPopDebugGroup", popDebugGroup);
    ASSIGN_WITH_EXT("GL_KHR_debug", "glObjectLabel", objectLabel);
    ASSIGN_WITH_EXT("GL_KHR_debug", "glGetObjectLabel", getObjectLabel);
    ASSIGN_WITH_EXT("GL_KHR_debug", "glObjectPtrLabel", objectPtrLabel);
    ASSIGN_WITH_EXT("GL_KHR_debug", "glGetObjectPtrLabel", getObjectPtrLabel);

    // GL_ARB_internalformat_query
    ASSIGN_WITH_EXT("GL_ARB_internalformat_query", "glGetInternalformativ", getInternalformativ);

    // GL_ARB_ES2_compatibility
    ASSIGN_WITH_EXT("GL_ARB_ES2_compatibility", "glReleaseShaderCompiler", releaseShaderCompiler);
    ASSIGN_WITH_EXT("GL_ARB_ES2_compatibility", "glShaderBinary", shaderBinary);
    ASSIGN_WITH_EXT("GL_ARB_ES2_compatibility", "glGetShaderPrecisionFormat", getShaderPrecisionFormat);
    ASSIGN_WITH_EXT("GL_ARB_ES2_compatibility", "glDepthRangef", depthRangef);
    ASSIGN_WITH_EXT("GL_ARB_ES2_compatibility", "glClearDepthf", clearDepthf);

    // GL_ARB_instanced_arrays
    ASSIGN_WITH_EXT("GL_ARB_instanced_arrays", "glVertexAttribDivisorARB", vertexAttribDivisor);

    // GL_EXT_draw_instanced
    ASSIGN_WITH_EXT("GL_EXT_draw_instanced", "glDrawArraysInstancedEXT", drawArraysInstanced);
    ASSIGN_WITH_EXT("GL_EXT_draw_instanced", "glDrawElementsInstancedEXT", drawElementsInstanced);

    // GL_ARB_draw_instanced
    ASSIGN_WITH_EXT("GL_ARB_draw_instanced", "glDrawArraysInstancedARB", drawArraysInstanced);
    ASSIGN_WITH_EXT("GL_ARB_draw_instanced", "glDrawElementsInstancedARB", drawElementsInstanced);

    // GL_ARB_sampler_objects
    ASSIGN_WITH_EXT("GL_ARB_sampler_objects", "glGenSamplers", genSamplers);
    ASSIGN_WITH_EXT("GL_ARB_sampler_objects", "glDeleteSamplers", deleteSamplers);
    ASSIGN_WITH_EXT("GL_ARB_sampler_objects", "glIsSampler", isSampler);
    ASSIGN_WITH_EXT("GL_ARB_sampler_objects", "glBindSampler", bindSampler);
    ASSIGN_WITH_EXT("GL_ARB_sampler_objects", "glSamplerParameteri", samplerParameteri);
    ASSIGN_WITH_EXT("GL_ARB_sampler_objects", "glSamplerParameterf", samplerParameterf);
    ASSIGN_WITH_EXT("GL_ARB_sampler_objects", "glSamplerParameteriv", samplerParameteriv);
    ASSIGN_WITH_EXT("GL_ARB_sampler_objects", "glSamplerParameterfv", samplerParameterfv);
    ASSIGN_WITH_EXT("GL_ARB_sampler_objects", "glSamplerParameterIiv", samplerParameterIiv);
    ASSIGN_WITH_EXT("GL_ARB_sampler_objects", "glSamplerParameterIuiv", samplerParameterIuiv);
    ASSIGN_WITH_EXT("GL_ARB_sampler_objects", "glGetSamplerParameteriv", getSamplerParameteriv);
    ASSIGN_WITH_EXT("GL_ARB_sampler_objects", "glGetSamplerParameterfv", getSamplerParameterfv);
    ASSIGN_WITH_EXT("GL_ARB_sampler_objects", "glGetSamplerParameterIiv", getSamplerParameterIiv);
    ASSIGN_WITH_EXT("GL_ARB_sampler_objects", "glGetSamplerParameterIuiv", getSamplerParameterIuiv);

    // GL_ARB_occlusion_query
    ASSIGN_WITH_EXT("GL_ARB_occlusion_query", "glGenQueriesARB", genQueries);
    ASSIGN_WITH_EXT("GL_ARB_occlusion_query", "glDeleteQueriesARB", deleteQueries);
    ASSIGN_WITH_EXT("GL_ARB_occlusion_query", "glIsQueryARB", isQuery);
    ASSIGN_WITH_EXT("GL_ARB_occlusion_query", "glBeginQueryARB", beginQuery);
    ASSIGN_WITH_EXT("GL_ARB_occlusion_query", "glEndQueryARB", endQuery);
    ASSIGN_WITH_EXT("GL_ARB_occlusion_query", "glGetQueryivARB", getQueryiv);
    ASSIGN_WITH_EXT("GL_ARB_occlusion_query", "glGetQueryObjectivARB", getQueryObjectiv);
    ASSIGN_WITH_EXT("GL_ARB_occlusion_query", "glGetQueryObjectuivARB", getQueryObjectuiv);

    // EXT_transform_feedback
    ASSIGN_WITH_EXT("EXT_transform_feedback", "glBindBufferRangeEXT", bindBufferRange);
    ASSIGN_WITH_EXT("EXT_transform_feedback", "glBindBufferBaseEXT", bindBufferBase);
    ASSIGN_WITH_EXT("EXT_transform_feedback", "glBeginTransformFeedbackEXT", beginTransformFeedback);
    ASSIGN_WITH_EXT("EXT_transform_feedback", "glEndTransformFeedbackEXT", endTransformFeedback);
    ASSIGN_WITH_EXT("EXT_transform_feedback", "glTransformFeedbackVaryingsEXT", transformFeedbackVaryings);
    ASSIGN_WITH_EXT("EXT_transform_feedback", "glGetTransformFeedbackVaryingEXT", getTransformFeedbackVarying);

    // GL_ARB_transform_feedback2
    ASSIGN_WITH_EXT("GL_ARB_transform_feedback2", "glBindTransformFeedback", bindTransformFeedback);
    ASSIGN_WITH_EXT("GL_ARB_transform_feedback2", "glDeleteTransformFeedbacks", deleteTransformFeedbacks);
    ASSIGN_WITH_EXT("GL_ARB_transform_feedback2", "glGenTransformFeedbacks", genTransformFeedbacks);
    ASSIGN_WITH_EXT("GL_ARB_transform_feedback2", "glIsTransformFeedback", isTransformFeedback);
    ASSIGN_WITH_EXT("GL_ARB_transform_feedback2", "glPauseTransformFeedback", pauseTransformFeedback);
    ASSIGN_WITH_EXT("GL_ARB_transform_feedback2", "glResumeTransformFeedback", resumeTransformFeedback);
    ASSIGN_WITH_EXT("GL_ARB_transform_feedback2", "glDrawTransformFeedback", drawTransformFeedback);

    // GL_ARB_transform_feedback3
    ASSIGN_WITH_EXT("GL_ARB_transform_feedback3", "glDrawTransformFeedbackStream", drawTransformFeedbackStream);
    ASSIGN_WITH_EXT("GL_ARB_transform_feedback3", "glBeginQueryIndexed", beginQueryIndexed);
    ASSIGN_WITH_EXT("GL_ARB_transform_feedback3", "glEndQueryIndexed", endQueryIndexed);
    ASSIGN_WITH_EXT("GL_ARB_transform_feedback3", "glGetQueryIndexediv", getQueryIndexediv);

    // GL_ARB_get_program_binary
    ASSIGN_WITH_EXT("GL_ARB_get_program_binary", "glGetProgramBinary", getProgramBinary);
    ASSIGN_WITH_EXT("GL_ARB_get_program_binary", "glProgramBinary", programBinary);
    ASSIGN_WITH_EXT("GL_ARB_get_program_binary", "glProgramParameteri", programParameteri);

    // GL_ARB_robustness
    ASSIGN_WITH_EXT("GL_ARB_robustness", "glGetGraphicsResetStatusARB", getGraphicsResetStatus);

    // GL_KHR_robustness
    ASSIGN_WITH_EXT("GL_KHR_robustness", "glGetGraphicsResetStatus", getGraphicsResetStatus);

    // GL_ARB_invalidate_subdata
    ASSIGN_WITH_EXT("GL_ARB_invalidate_subdata", "glInvalidateTexSubImage", invalidateTexSubImage);
    ASSIGN_WITH_EXT("GL_ARB_invalidate_subdata", "glInvalidateTexImage", invalidateTexImage);
    ASSIGN_WITH_EXT("GL_ARB_invalidate_subdata", "glInvalidateBufferSubData", invalidateBufferSubData);
    ASSIGN_WITH_EXT("GL_ARB_invalidate_subdata", "glInvalidateBufferData", invalidateBufferData);
    ASSIGN_WITH_EXT("GL_ARB_invalidate_subdata", "glInvalidateFramebuffer", invalidateFramebuffer);
    ASSIGN_WITH_EXT("GL_ARB_invalidate_subdata", "glInvalidateSubFramebuffer", invalidateSubFramebuffer);

    // 1.0
    if (isAtLeastGL(gl::Version(1, 0)))
    {
        ASSIGN("glBlendFunc", blendFunc);
        ASSIGN("glClear", clear);
        ASSIGN("glClearColor", clearColor);
        ASSIGN("glClearDepth", clearDepth);
        ASSIGN("glClearStencil", clearStencil);
        ASSIGN("glColorMask", colorMask);
        ASSIGN("glCullFace", cullFace);
        ASSIGN("glDepthFunc", depthFunc);
        ASSIGN("glDepthMask", depthMask);
        ASSIGN("glDepthRange", depthRange);
        ASSIGN("glDisable", disable);
        ASSIGN("glDrawBuffer", drawBuffer);
        ASSIGN("glEnable", enable);
        ASSIGN("glFinish", finish);
        ASSIGN("glFlush", flush);
        ASSIGN("glFrontFace", frontFace);
        ASSIGN("glGetBooleanv", getBooleanv);
        ASSIGN("glGetDoublev", getDoublev);
        ASSIGN("glGetError", getError);
        ASSIGN("glGetFloatv", getFloatv);
        ASSIGN("glGetIntegerv", getIntegerv);
        ASSIGN("glGetString", getString);
        ASSIGN("glGetTexImage", getTexImage);
        ASSIGN("glGetTexLevelParameterfv", getTexLevelParameterfv);
        ASSIGN("glGetTexLevelParameteriv", getTexLevelParameteriv);
        ASSIGN("glGetTexParameterfv", getTexParameterfv);
        ASSIGN("glGetTexParameteriv", getTexParameteriv);
        ASSIGN("glHint", hint);
        ASSIGN("glIsEnabled", isEnabled);
        ASSIGN("glLineWidth", lineWidth);
        ASSIGN("glLogicOp", logicOp);
        ASSIGN("glPixelStoref", pixelStoref);
        ASSIGN("glPixelStorei", pixelStorei);
        ASSIGN("glPointSize", pointSize);
        ASSIGN("glPolygonMode", polygonMode);
        ASSIGN("glReadBuffer", readBuffer);
        ASSIGN("glReadPixels", readPixels);
        ASSIGN("glScissor", scissor);
        ASSIGN("glStencilFunc", stencilFunc);
        ASSIGN("glStencilMask", stencilMask);
        ASSIGN("glStencilOp", stencilOp);
        ASSIGN("glTexImage1D", texImage1D);
        ASSIGN("glTexImage2D", texImage2D);
        ASSIGN("glTexParameterf", texParameterf);
        ASSIGN("glTexParameterfv", texParameterfv);
        ASSIGN("glTexParameteri", texParameteri);
        ASSIGN("glTexParameteriv", texParameteriv);
        ASSIGN("glViewport", viewport);
    }

    // 1.1
    if (isAtLeastGL(gl::Version(1, 1)))
    {
        ASSIGN("glBindTexture", bindTexture);
        ASSIGN("glCopyTexImage1D", copyTexImage1D);
        ASSIGN("glCopyTexImage2D", copyTexImage2D);
        ASSIGN("glCopyTexSubImage1D", copyTexSubImage1D);
        ASSIGN("glCopyTexSubImage2D", copyTexSubImage2D);
        ASSIGN("glDeleteTextures", deleteTextures);
        ASSIGN("glDrawArrays", drawArrays);
        ASSIGN("glDrawElements", drawElements);
        ASSIGN("glGenTextures", genTextures);
        ASSIGN("glIsTexture", isTexture);
        ASSIGN("glPolygonOffset", polygonOffset);
        ASSIGN("glTexSubImage1D", texSubImage1D);
        ASSIGN("glTexSubImage2D", texSubImage2D);
    }

    // 1.2
    if (isAtLeastGL(gl::Version(1, 2)))
    {
        ASSIGN("glBlendColor", blendColor);
        ASSIGN("glBlendEquation", blendEquation);
        ASSIGN("glCopyTexSubImage3D", copyTexSubImage3D);
        ASSIGN("glDrawRangeElements", drawRangeElements);
        ASSIGN("glTexImage3D", texImage3D);
        ASSIGN("glTexSubImage3D", texSubImage3D);
    }

    // 1.3
    if (isAtLeastGL(gl::Version(1, 3)))
    {
        ASSIGN("glActiveTexture", activeTexture);
        ASSIGN("glCompressedTexImage1D", compressedTexImage1D);
        ASSIGN("glCompressedTexImage2D", compressedTexImage2D);
        ASSIGN("glCompressedTexImage3D", compressedTexImage3D);
        ASSIGN("glCompressedTexSubImage1D", compressedTexSubImage1D);
        ASSIGN("glCompressedTexSubImage2D", compressedTexSubImage2D);
        ASSIGN("glCompressedTexSubImage3D", compressedTexSubImage3D);
        ASSIGN("glGetCompressedTexImage", getCompressedTexImage);
        ASSIGN("glSampleCoverage", sampleCoverage);
    }

    // 1.4
    if (isAtLeastGL(gl::Version(1, 4)))
    {
        ASSIGN("glBlendFuncSeparate", blendFuncSeparate);
        ASSIGN("glMultiDrawArrays", multiDrawArrays);
        ASSIGN("glMultiDrawElements", multiDrawElements);
        ASSIGN("glPointParameterf", pointParameterf);
        ASSIGN("glPointParameterfv", pointParameterfv);
        ASSIGN("glPointParameteri", pointParameteri);
        ASSIGN("glPointParameteriv", pointParameteriv);
    }

    // 1.5
    if (isAtLeastGL(gl::Version(1, 5)))
    {
        ASSIGN("glBeginQuery", beginQuery);
        ASSIGN("glBindBuffer", bindBuffer);
        ASSIGN("glBufferData", bufferData);
        ASSIGN("glBufferSubData", bufferSubData);
        ASSIGN("glDeleteBuffers", deleteBuffers);
        ASSIGN("glDeleteQueries", deleteQueries);
        ASSIGN("glEndQuery", endQuery);
        ASSIGN("glGenBuffers", genBuffers);
        ASSIGN("glGenQueries", genQueries);
        ASSIGN("glGetBufferParameteriv", getBufferParameteriv);
        ASSIGN("glGetBufferPointerv", getBufferPointerv);
        ASSIGN("glGetBufferSubData", getBufferSubData);
        ASSIGN("glGetQueryObjectiv", getQueryObjectiv);
        ASSIGN("glGetQueryObjectuiv", getQueryObjectuiv);
        ASSIGN("glGetQueryiv", getQueryiv);
        ASSIGN("glIsBuffer", isBuffer);
        ASSIGN("glIsQuery", isQuery);
        ASSIGN("glMapBuffer", mapBuffer);
        ASSIGN("glUnmapBuffer", unmapBuffer);
    }

    // 2.0
    if (isAtLeastGL(gl::Version(2, 0)))
    {
        ASSIGN("glAttachShader", attachShader);
        ASSIGN("glBindAttribLocation", bindAttribLocation);
        ASSIGN("glBlendEquationSeparate", blendEquationSeparate);
        ASSIGN("glCompileShader", compileShader);
        ASSIGN("glCreateProgram", createProgram);
        ASSIGN("glCreateShader", createShader);
        ASSIGN("glDeleteProgram", deleteProgram);
        ASSIGN("glDeleteShader", deleteShader);
        ASSIGN("glDetachShader", detachShader);
        ASSIGN("glDisableVertexAttribArray", disableVertexAttribArray);
        ASSIGN("glDrawBuffers", drawBuffers);
        ASSIGN("glEnableVertexAttribArray", enableVertexAttribArray);
        ASSIGN("glGetActiveAttrib", getActiveAttrib);
        ASSIGN("glGetActiveUniform", getActiveUniform);
        ASSIGN("glGetAttachedShaders", getAttachedShaders);
        ASSIGN("glGetAttribLocation", getAttribLocation);
        ASSIGN("glGetProgramInfoLog", getProgramInfoLog);
        ASSIGN("glGetProgramiv", getProgramiv);
        ASSIGN("glGetShaderInfoLog", getShaderInfoLog);
        ASSIGN("glGetShaderSource", getShaderSource);
        ASSIGN("glGetShaderiv", getShaderiv);
        ASSIGN("glGetUniformLocation", getUniformLocation);
        ASSIGN("glGetUniformfv", getUniformfv);
        ASSIGN("glGetUniformiv", getUniformiv);
        ASSIGN("glGetVertexAttribPointerv", getVertexAttribPointerv);
        ASSIGN("glGetVertexAttribdv", getVertexAttribdv);
        ASSIGN("glGetVertexAttribfv", getVertexAttribfv);
        ASSIGN("glGetVertexAttribiv", getVertexAttribiv);
        ASSIGN("glIsProgram", isProgram);
        ASSIGN("glIsShader", isShader);
        ASSIGN("glLinkProgram", linkProgram);
        ASSIGN("glShaderSource", shaderSource);
        ASSIGN("glStencilFuncSeparate", stencilFuncSeparate);
        ASSIGN("glStencilMaskSeparate", stencilMaskSeparate);
        ASSIGN("glStencilOpSeparate", stencilOpSeparate);
        ASSIGN("glUniform1f", uniform1f);
        ASSIGN("glUniform1fv", uniform1fv);
        ASSIGN("glUniform1i", uniform1i);
        ASSIGN("glUniform1iv", uniform1iv);
        ASSIGN("glUniform2f", uniform2f);
        ASSIGN("glUniform2fv", uniform2fv);
        ASSIGN("glUniform2i", uniform2i);
        ASSIGN("glUniform2iv", uniform2iv);
        ASSIGN("glUniform3f", uniform3f);
        ASSIGN("glUniform3fv", uniform3fv);
        ASSIGN("glUniform3i", uniform3i);
        ASSIGN("glUniform3iv", uniform3iv);
        ASSIGN("glUniform4f", uniform4f);
        ASSIGN("glUniform4fv", uniform4fv);
        ASSIGN("glUniform4i", uniform4i);
        ASSIGN("glUniform4iv", uniform4iv);
        ASSIGN("glUniformMatrix2fv", uniformMatrix2fv);
        ASSIGN("glUniformMatrix3fv", uniformMatrix3fv);
        ASSIGN("glUniformMatrix4fv", uniformMatrix4fv);
        ASSIGN("glUseProgram", useProgram);
        ASSIGN("glValidateProgram", validateProgram);
        ASSIGN("glVertexAttrib1d", vertexAttrib1d);
        ASSIGN("glVertexAttrib1dv", vertexAttrib1dv);
        ASSIGN("glVertexAttrib1f", vertexAttrib1f);
        ASSIGN("glVertexAttrib1fv", vertexAttrib1fv);
        ASSIGN("glVertexAttrib1s", vertexAttrib1s);
        ASSIGN("glVertexAttrib1sv", vertexAttrib1sv);
        ASSIGN("glVertexAttrib2d", vertexAttrib2d);
        ASSIGN("glVertexAttrib2dv", vertexAttrib2dv);
        ASSIGN("glVertexAttrib2f", vertexAttrib2f);
        ASSIGN("glVertexAttrib2fv", vertexAttrib2fv);
        ASSIGN("glVertexAttrib2s", vertexAttrib2s);
        ASSIGN("glVertexAttrib2sv", vertexAttrib2sv);
        ASSIGN("glVertexAttrib3d", vertexAttrib3d);
        ASSIGN("glVertexAttrib3dv", vertexAttrib3dv);
        ASSIGN("glVertexAttrib3f", vertexAttrib3f);
        ASSIGN("glVertexAttrib3fv", vertexAttrib3fv);
        ASSIGN("glVertexAttrib3s", vertexAttrib3s);
        ASSIGN("glVertexAttrib3sv", vertexAttrib3sv);
        ASSIGN("glVertexAttrib4Nbv", vertexAttrib4Nbv);
        ASSIGN("glVertexAttrib4Niv", vertexAttrib4Niv);
        ASSIGN("glVertexAttrib4Nsv", vertexAttrib4Nsv);
        ASSIGN("glVertexAttrib4Nub", vertexAttrib4Nub);
        ASSIGN("glVertexAttrib4Nubv", vertexAttrib4Nubv);
        ASSIGN("glVertexAttrib4Nuiv", vertexAttrib4Nuiv);
        ASSIGN("glVertexAttrib4Nusv", vertexAttrib4Nusv);
        ASSIGN("glVertexAttrib4bv", vertexAttrib4bv);
        ASSIGN("glVertexAttrib4d", vertexAttrib4d);
        ASSIGN("glVertexAttrib4dv", vertexAttrib4dv);
        ASSIGN("glVertexAttrib4f", vertexAttrib4f);
        ASSIGN("glVertexAttrib4fv", vertexAttrib4fv);
        ASSIGN("glVertexAttrib4iv", vertexAttrib4iv);
        ASSIGN("glVertexAttrib4s", vertexAttrib4s);
        ASSIGN("glVertexAttrib4sv", vertexAttrib4sv);
        ASSIGN("glVertexAttrib4ubv", vertexAttrib4ubv);
        ASSIGN("glVertexAttrib4uiv", vertexAttrib4uiv);
        ASSIGN("glVertexAttrib4usv", vertexAttrib4usv);
        ASSIGN("glVertexAttribPointer", vertexAttribPointer);
    }

    // 2.1
    if (isAtLeastGL(gl::Version(2, 1)))
    {
        ASSIGN("glUniformMatrix2x3fv", uniformMatrix2x3fv);
        ASSIGN("glUniformMatrix2x4fv", uniformMatrix2x4fv);
        ASSIGN("glUniformMatrix3x2fv", uniformMatrix3x2fv);
        ASSIGN("glUniformMatrix3x4fv", uniformMatrix3x4fv);
        ASSIGN("glUniformMatrix4x2fv", uniformMatrix4x2fv);
        ASSIGN("glUniformMatrix4x3fv", uniformMatrix4x3fv);
    }

    // 3.0
    if (isAtLeastGL(gl::Version(3, 0)))
    {
        ASSIGN("glBeginConditionalRender", beginConditionalRender);
        ASSIGN("glBeginTransformFeedback", beginTransformFeedback);
        ASSIGN("glBindBufferBase", bindBufferBase);
        ASSIGN("glBindBufferRange", bindBufferRange);
        ASSIGN("glBindFragDataLocation", bindFragDataLocation);
        ASSIGN("glBindFramebuffer", bindFramebuffer);
        ASSIGN("glBindRenderbuffer", bindRenderbuffer);
        ASSIGN("glBindVertexArray", bindVertexArray);
        ASSIGN("glBlitFramebuffer", blitFramebuffer);
        ASSIGN("glCheckFramebufferStatus", checkFramebufferStatus);
        ASSIGN("glClampColor", clampColor);
        ASSIGN("glClearBufferfi", clearBufferfi);
        ASSIGN("glClearBufferfv", clearBufferfv);
        ASSIGN("glClearBufferiv", clearBufferiv);
        ASSIGN("glClearBufferuiv", clearBufferuiv);
        ASSIGN("glColorMaski", colorMaski);
        ASSIGN("glDeleteFramebuffers", deleteFramebuffers);
        ASSIGN("glDeleteRenderbuffers", deleteRenderbuffers);
        ASSIGN("glDeleteVertexArrays", deleteVertexArrays);
        ASSIGN("glDisablei", disablei);
        ASSIGN("glEnablei", enablei);
        ASSIGN("glEndConditionalRender", endConditionalRender);
        ASSIGN("glEndTransformFeedback", endTransformFeedback);
        ASSIGN("glFlushMappedBufferRange", flushMappedBufferRange);
        ASSIGN("glFramebufferRenderbuffer", framebufferRenderbuffer);
        ASSIGN("glFramebufferTexture1D", framebufferTexture1D);
        ASSIGN("glFramebufferTexture2D", framebufferTexture2D);
        ASSIGN("glFramebufferTexture3D", framebufferTexture3D);
        ASSIGN("glFramebufferTextureLayer", framebufferTextureLayer);
        ASSIGN("glGenFramebuffers", genFramebuffers);
        ASSIGN("glGenRenderbuffers", genRenderbuffers);
        ASSIGN("glGenVertexArrays", genVertexArrays);
        ASSIGN("glGenerateMipmap", generateMipmap);
        ASSIGN("glGetBooleani_v", getBooleani_v);
        ASSIGN("glGetFragDataLocation", getFragDataLocation);
        ASSIGN("glGetFramebufferAttachmentParameteriv", getFramebufferAttachmentParameteriv);
        ASSIGN("glGetIntegeri_v", getIntegeri_v);
        ASSIGN("glGetRenderbufferParameteriv", getRenderbufferParameteriv);
        ASSIGN("glGetStringi", getStringi);
        ASSIGN("glGetTexParameterIiv", getTexParameterIiv);
        ASSIGN("glGetTexParameterIuiv", getTexParameterIuiv);
        ASSIGN("glGetTransformFeedbackVarying", getTransformFeedbackVarying);
        ASSIGN("glGetUniformuiv", getUniformuiv);
        ASSIGN("glGetVertexAttribIiv", getVertexAttribIiv);
        ASSIGN("glGetVertexAttribIuiv", getVertexAttribIuiv);
        ASSIGN("glIsEnabledi", isEnabledi);
        ASSIGN("glIsFramebuffer", isFramebuffer);
        ASSIGN("glIsRenderbuffer", isRenderbuffer);
        ASSIGN("glIsVertexArray", isVertexArray);
        ASSIGN("glMapBufferRange", mapBufferRange);
        ASSIGN("glRenderbufferStorage", renderbufferStorage);
        ASSIGN("glRenderbufferStorageMultisample", renderbufferStorageMultisample);
        ASSIGN("glTexParameterIiv", texParameterIiv);
        ASSIGN("glTexParameterIuiv", texParameterIuiv);
        ASSIGN("glTransformFeedbackVaryings", transformFeedbackVaryings);
        ASSIGN("glUniform1ui", uniform1ui);
        ASSIGN("glUniform1uiv", uniform1uiv);
        ASSIGN("glUniform2ui", uniform2ui);
        ASSIGN("glUniform2uiv", uniform2uiv);
        ASSIGN("glUniform3ui", uniform3ui);
        ASSIGN("glUniform3uiv", uniform3uiv);
        ASSIGN("glUniform4ui", uniform4ui);
        ASSIGN("glUniform4uiv", uniform4uiv);
        ASSIGN("glVertexAttribI1i", vertexAttribI1i);
        ASSIGN("glVertexAttribI1iv", vertexAttribI1iv);
        ASSIGN("glVertexAttribI1ui", vertexAttribI1ui);
        ASSIGN("glVertexAttribI1uiv", vertexAttribI1uiv);
        ASSIGN("glVertexAttribI2i", vertexAttribI2i);
        ASSIGN("glVertexAttribI2iv", vertexAttribI2iv);
        ASSIGN("glVertexAttribI2ui", vertexAttribI2ui);
        ASSIGN("glVertexAttribI2uiv", vertexAttribI2uiv);
        ASSIGN("glVertexAttribI3i", vertexAttribI3i);
        ASSIGN("glVertexAttribI3iv", vertexAttribI3iv);
        ASSIGN("glVertexAttribI3ui", vertexAttribI3ui);
        ASSIGN("glVertexAttribI3uiv", vertexAttribI3uiv);
        ASSIGN("glVertexAttribI4bv", vertexAttribI4bv);
        ASSIGN("glVertexAttribI4i", vertexAttribI4i);
        ASSIGN("glVertexAttribI4iv", vertexAttribI4iv);
        ASSIGN("glVertexAttribI4sv", vertexAttribI4sv);
        ASSIGN("glVertexAttribI4ubv", vertexAttribI4ubv);
        ASSIGN("glVertexAttribI4ui", vertexAttribI4ui);
        ASSIGN("glVertexAttribI4uiv", vertexAttribI4uiv);
        ASSIGN("glVertexAttribI4usv", vertexAttribI4usv);
        ASSIGN("glVertexAttribIPointer", vertexAttribIPointer);
    }

    // 3.1
    if (isAtLeastGL(gl::Version(3, 1)))
    {
        ASSIGN("glCopyBufferSubData", copyBufferSubData);
        ASSIGN("glDrawArraysInstanced", drawArraysInstanced);
        ASSIGN("glDrawElementsInstanced", drawElementsInstanced);
        ASSIGN("glGetActiveUniformBlockName", getActiveUniformBlockName);
        ASSIGN("glGetActiveUniformBlockiv", getActiveUniformBlockiv);
        ASSIGN("glGetActiveUniformName", getActiveUniformName);
        ASSIGN("glGetActiveUniformsiv", getActiveUniformsiv);
        ASSIGN("glGetUniformBlockIndex", getUniformBlockIndex);
        ASSIGN("glGetUniformIndices", getUniformIndices);
        ASSIGN("glPrimitiveRestartIndex", primitiveRestartIndex);
        ASSIGN("glTexBuffer", texBuffer);
        ASSIGN("glUniformBlockBinding", uniformBlockBinding);
    }

    // 3.2
    if (isAtLeastGL(gl::Version(3, 2)))
    {
        ASSIGN("glClientWaitSync", clientWaitSync);
        ASSIGN("glDeleteSync", deleteSync);
        ASSIGN("glDrawElementsBaseVertex", drawElementsBaseVertex);
        ASSIGN("glDrawElementsInstancedBaseVertex", drawElementsInstancedBaseVertex);
        ASSIGN("glDrawRangeElementsBaseVertex", drawRangeElementsBaseVertex);
        ASSIGN("glFenceSync", fenceSync);
        ASSIGN("glFramebufferTexture", framebufferTexture);
        ASSIGN("glGetBufferParameteri64v", getBufferParameteri64v);
        ASSIGN("glGetInteger64i_v", getInteger64i_v);
        ASSIGN("glGetInteger64v", getInteger64v);
        ASSIGN("glGetMultisamplefv", getMultisamplefv);
        ASSIGN("glGetSynciv", getSynciv);
        ASSIGN("glIsSync", isSync);
        ASSIGN("glMultiDrawElementsBaseVertex", multiDrawElementsBaseVertex);
        ASSIGN("glProvokingVertex", provokingVertex);
        ASSIGN("glSampleMaski", sampleMaski);
        ASSIGN("glTexImage2DMultisample", texImage2DMultisample);
        ASSIGN("glTexImage3DMultisample", texImage3DMultisample);
        ASSIGN("glWaitSync", waitSync);
    }

    // 3.3
    if (isAtLeastGL(gl::Version(3, 3)))
    {
        ASSIGN("glBindFragDataLocationIndexed", bindFragDataLocationIndexed);
        ASSIGN("glBindSampler", bindSampler);
        ASSIGN("glDeleteSamplers", deleteSamplers);
        ASSIGN("glGenSamplers", genSamplers);
        ASSIGN("glGetFragDataIndex", getFragDataIndex);
        ASSIGN("glGetQueryObjecti64v", getQueryObjecti64v);
        ASSIGN("glGetQueryObjectui64v", getQueryObjectui64v);
        ASSIGN("glGetSamplerParameterIiv", getSamplerParameterIiv);
        ASSIGN("glGetSamplerParameterIuiv", getSamplerParameterIuiv);
        ASSIGN("glGetSamplerParameterfv", getSamplerParameterfv);
        ASSIGN("glGetSamplerParameteriv", getSamplerParameteriv);
        ASSIGN("glIsSampler", isSampler);
        ASSIGN("glQueryCounter", queryCounter);
        ASSIGN("glSamplerParameterIiv", samplerParameterIiv);
        ASSIGN("glSamplerParameterIuiv", samplerParameterIuiv);
        ASSIGN("glSamplerParameterf", samplerParameterf);
        ASSIGN("glSamplerParameterfv", samplerParameterfv);
        ASSIGN("glSamplerParameteri", samplerParameteri);
        ASSIGN("glSamplerParameteriv", samplerParameteriv);
        ASSIGN("glVertexAttribDivisor", vertexAttribDivisor);
        ASSIGN("glVertexAttribP1ui", vertexAttribP1ui);
        ASSIGN("glVertexAttribP1uiv", vertexAttribP1uiv);
        ASSIGN("glVertexAttribP2ui", vertexAttribP2ui);
        ASSIGN("glVertexAttribP2uiv", vertexAttribP2uiv);
        ASSIGN("glVertexAttribP3ui", vertexAttribP3ui);
        ASSIGN("glVertexAttribP3uiv", vertexAttribP3uiv);
        ASSIGN("glVertexAttribP4ui", vertexAttribP4ui);
        ASSIGN("glVertexAttribP4uiv", vertexAttribP4uiv);
    }

    // 4.0
    if (isAtLeastGL(gl::Version(4, 0)))
    {
        ASSIGN("glBeginQueryIndexed", beginQueryIndexed);
        ASSIGN("glBindTransformFeedback", bindTransformFeedback);
        ASSIGN("glBlendEquationSeparatei", blendEquationSeparatei);
        ASSIGN("glBlendEquationi", blendEquationi);
        ASSIGN("glBlendFuncSeparatei", blendFuncSeparatei);
        ASSIGN("glBlendFunci", blendFunci);
        ASSIGN("glDeleteTransformFeedbacks", deleteTransformFeedbacks);
        ASSIGN("glDrawArraysIndirect", drawArraysIndirect);
        ASSIGN("glDrawElementsIndirect", drawElementsIndirect);
        ASSIGN("glDrawTransformFeedback", drawTransformFeedback);
        ASSIGN("glDrawTransformFeedbackStream", drawTransformFeedbackStream);
        ASSIGN("glEndQueryIndexed", endQueryIndexed);
        ASSIGN("glGenTransformFeedbacks", genTransformFeedbacks);
        ASSIGN("glGetActiveSubroutineName", getActiveSubroutineName);
        ASSIGN("glGetActiveSubroutineUniformName", getActiveSubroutineUniformName);
        ASSIGN("glGetActiveSubroutineUniformiv", getActiveSubroutineUniformiv);
        ASSIGN("glGetProgramStageiv", getProgramStageiv);
        ASSIGN("glGetQueryIndexediv", getQueryIndexediv);
        ASSIGN("glGetSubroutineIndex", getSubroutineIndex);
        ASSIGN("glGetSubroutineUniformLocation", getSubroutineUniformLocation);
        ASSIGN("glGetUniformSubroutineuiv", getUniformSubroutineuiv);
        ASSIGN("glGetUniformdv", getUniformdv);
        ASSIGN("glIsTransformFeedback", isTransformFeedback);
        ASSIGN("glMinSampleShading", minSampleShading);
        ASSIGN("glPatchParameterfv", patchParameterfv);
        ASSIGN("glPatchParameteri", patchParameteri);
        ASSIGN("glPauseTransformFeedback", pauseTransformFeedback);
        ASSIGN("glResumeTransformFeedback", resumeTransformFeedback);
        ASSIGN("glUniform1d", uniform1d);
        ASSIGN("glUniform1dv", uniform1dv);
        ASSIGN("glUniform2d", uniform2d);
        ASSIGN("glUniform2dv", uniform2dv);
        ASSIGN("glUniform3d", uniform3d);
        ASSIGN("glUniform3dv", uniform3dv);
        ASSIGN("glUniform4d", uniform4d);
        ASSIGN("glUniform4dv", uniform4dv);
        ASSIGN("glUniformMatrix2dv", uniformMatrix2dv);
        ASSIGN("glUniformMatrix2x3dv", uniformMatrix2x3dv);
        ASSIGN("glUniformMatrix2x4dv", uniformMatrix2x4dv);
        ASSIGN("glUniformMatrix3dv", uniformMatrix3dv);
        ASSIGN("glUniformMatrix3x2dv", uniformMatrix3x2dv);
        ASSIGN("glUniformMatrix3x4dv", uniformMatrix3x4dv);
        ASSIGN("glUniformMatrix4dv", uniformMatrix4dv);
        ASSIGN("glUniformMatrix4x2dv", uniformMatrix4x2dv);
        ASSIGN("glUniformMatrix4x3dv", uniformMatrix4x3dv);
        ASSIGN("glUniformSubroutinesuiv", uniformSubroutinesuiv);
    }

    // 4.1
    if (isAtLeastGL(gl::Version(4, 1)))
    {
        ASSIGN("glActiveShaderProgram", activeShaderProgram);
        ASSIGN("glBindProgramPipeline", bindProgramPipeline);
        ASSIGN("glClearDepthf", clearDepthf);
        ASSIGN("glCreateShaderProgramv", createShaderProgramv);
        ASSIGN("glDeleteProgramPipelines", deleteProgramPipelines);
        ASSIGN("glDepthRangeArrayv", depthRangeArrayv);
        ASSIGN("glDepthRangeIndexed", depthRangeIndexed);
        ASSIGN("glDepthRangef", depthRangef);
        ASSIGN("glGenProgramPipelines", genProgramPipelines);
        ASSIGN("glGetDoublei_v", getDoublei_v);
        ASSIGN("glGetFloati_v", getFloati_v);
        ASSIGN("glGetProgramBinary", getProgramBinary);
        ASSIGN("glGetProgramPipelineInfoLog", getProgramPipelineInfoLog);
        ASSIGN("glGetProgramPipelineiv", getProgramPipelineiv);
        ASSIGN("glGetShaderPrecisionFormat", getShaderPrecisionFormat);
        ASSIGN("glGetVertexAttribLdv", getVertexAttribLdv);
        ASSIGN("glIsProgramPipeline", isProgramPipeline);
        ASSIGN("glProgramBinary", programBinary);
        ASSIGN("glProgramParameteri", programParameteri);
        ASSIGN("glProgramUniform1d", programUniform1d);
        ASSIGN("glProgramUniform1dv", programUniform1dv);
        ASSIGN("glProgramUniform1f", programUniform1f);
        ASSIGN("glProgramUniform1fv", programUniform1fv);
        ASSIGN("glProgramUniform1i", programUniform1i);
        ASSIGN("glProgramUniform1iv", programUniform1iv);
        ASSIGN("glProgramUniform1ui", programUniform1ui);
        ASSIGN("glProgramUniform1uiv", programUniform1uiv);
        ASSIGN("glProgramUniform2d", programUniform2d);
        ASSIGN("glProgramUniform2dv", programUniform2dv);
        ASSIGN("glProgramUniform2f", programUniform2f);
        ASSIGN("glProgramUniform2fv", programUniform2fv);
        ASSIGN("glProgramUniform2i", programUniform2i);
        ASSIGN("glProgramUniform2iv", programUniform2iv);
        ASSIGN("glProgramUniform2ui", programUniform2ui);
        ASSIGN("glProgramUniform2uiv", programUniform2uiv);
        ASSIGN("glProgramUniform3d", programUniform3d);
        ASSIGN("glProgramUniform3dv", programUniform3dv);
        ASSIGN("glProgramUniform3f", programUniform3f);
        ASSIGN("glProgramUniform3fv", programUniform3fv);
        ASSIGN("glProgramUniform3i", programUniform3i);
        ASSIGN("glProgramUniform3iv", programUniform3iv);
        ASSIGN("glProgramUniform3ui", programUniform3ui);
        ASSIGN("glProgramUniform3uiv", programUniform3uiv);
        ASSIGN("glProgramUniform4d", programUniform4d);
        ASSIGN("glProgramUniform4dv", programUniform4dv);
        ASSIGN("glProgramUniform4f", programUniform4f);
        ASSIGN("glProgramUniform4fv", programUniform4fv);
        ASSIGN("glProgramUniform4i", programUniform4i);
        ASSIGN("glProgramUniform4iv", programUniform4iv);
        ASSIGN("glProgramUniform4ui", programUniform4ui);
        ASSIGN("glProgramUniform4uiv", programUniform4uiv);
        ASSIGN("glProgramUniformMatrix2dv", programUniformMatrix2dv);
        ASSIGN("glProgramUniformMatrix2fv", programUniformMatrix2fv);
        ASSIGN("glProgramUniformMatrix2x3dv", programUniformMatrix2x3dv);
        ASSIGN("glProgramUniformMatrix2x3fv", programUniformMatrix2x3fv);
        ASSIGN("glProgramUniformMatrix2x4dv", programUniformMatrix2x4dv);
        ASSIGN("glProgramUniformMatrix2x4fv", programUniformMatrix2x4fv);
        ASSIGN("glProgramUniformMatrix3dv", programUniformMatrix3dv);
        ASSIGN("glProgramUniformMatrix3fv", programUniformMatrix3fv);
        ASSIGN("glProgramUniformMatrix3x2dv", programUniformMatrix3x2dv);
        ASSIGN("glProgramUniformMatrix3x2fv", programUniformMatrix3x2fv);
        ASSIGN("glProgramUniformMatrix3x4dv", programUniformMatrix3x4dv);
        ASSIGN("glProgramUniformMatrix3x4fv", programUniformMatrix3x4fv);
        ASSIGN("glProgramUniformMatrix4dv", programUniformMatrix4dv);
        ASSIGN("glProgramUniformMatrix4fv", programUniformMatrix4fv);
        ASSIGN("glProgramUniformMatrix4x2dv", programUniformMatrix4x2dv);
        ASSIGN("glProgramUniformMatrix4x2fv", programUniformMatrix4x2fv);
        ASSIGN("glProgramUniformMatrix4x3dv", programUniformMatrix4x3dv);
        ASSIGN("glProgramUniformMatrix4x3fv", programUniformMatrix4x3fv);
        ASSIGN("glReleaseShaderCompiler", releaseShaderCompiler);
        ASSIGN("glScissorArrayv", scissorArrayv);
        ASSIGN("glScissorIndexed", scissorIndexed);
        ASSIGN("glScissorIndexedv", scissorIndexedv);
        ASSIGN("glShaderBinary", shaderBinary);
        ASSIGN("glUseProgramStages", useProgramStages);
        ASSIGN("glValidateProgramPipeline", validateProgramPipeline);
        ASSIGN("glVertexAttribL1d", vertexAttribL1d);
        ASSIGN("glVertexAttribL1dv", vertexAttribL1dv);
        ASSIGN("glVertexAttribL2d", vertexAttribL2d);
        ASSIGN("glVertexAttribL2dv", vertexAttribL2dv);
        ASSIGN("glVertexAttribL3d", vertexAttribL3d);
        ASSIGN("glVertexAttribL3dv", vertexAttribL3dv);
        ASSIGN("glVertexAttribL4d", vertexAttribL4d);
        ASSIGN("glVertexAttribL4dv", vertexAttribL4dv);
        ASSIGN("glVertexAttribLPointer", vertexAttribLPointer);
        ASSIGN("glViewportArrayv", viewportArrayv);
        ASSIGN("glViewportIndexedf", viewportIndexedf);
        ASSIGN("glViewportIndexedfv", viewportIndexedfv);
    }

    // 4.2
    if (isAtLeastGL(gl::Version(4, 2)))
    {
        ASSIGN("glBindImageTexture", bindImageTexture);
        ASSIGN("glDrawArraysInstancedBaseInstance", drawArraysInstancedBaseInstance);
        ASSIGN("glDrawElementsInstancedBaseInstance", drawElementsInstancedBaseInstance);
        ASSIGN("glDrawElementsInstancedBaseVertexBaseInstance", drawElementsInstancedBaseVertexBaseInstance);
        ASSIGN("glDrawTransformFeedbackInstanced", drawTransformFeedbackInstanced);
        ASSIGN("glDrawTransformFeedbackStreamInstanced", drawTransformFeedbackStreamInstanced);
        ASSIGN("glGetActiveAtomicCounterBufferiv", getActiveAtomicCounterBufferiv);
        ASSIGN("glGetInternalformativ", getInternalformativ);
        ASSIGN("glMemoryBarrier", memoryBarrier);
        ASSIGN("glTexStorage1D", texStorage1D);
        ASSIGN("glTexStorage2D", texStorage2D);
        ASSIGN("glTexStorage3D", texStorage3D);
    }

    // 4.3
    if (isAtLeastGL(gl::Version(4, 3)))
    {
        ASSIGN("glBindVertexBuffer", bindVertexBuffer);
        ASSIGN("glClearBufferData", clearBufferData);
        ASSIGN("glClearBufferSubData", clearBufferSubData);
        ASSIGN("glCopyImageSubData", copyImageSubData);
        ASSIGN("glDebugMessageCallback", debugMessageCallback);
        ASSIGN("glDebugMessageControl", debugMessageControl);
        ASSIGN("glDebugMessageInsert", debugMessageInsert);
        ASSIGN("glDispatchCompute", dispatchCompute);
        ASSIGN("glDispatchComputeIndirect", dispatchComputeIndirect);
        ASSIGN("glFramebufferParameteri", framebufferParameteri);
        ASSIGN("glGetDebugMessageLog", getDebugMessageLog);
        ASSIGN("glGetFramebufferParameteriv", getFramebufferParameteriv);
        ASSIGN("glGetInternalformati64v", getInternalformati64v);
        ASSIGN("glGetPointerv", getPointerv);
        ASSIGN("glGetObjectLabel", getObjectLabel);
        ASSIGN("glGetObjectPtrLabel", getObjectPtrLabel);
        ASSIGN("glGetProgramInterfaceiv", getProgramInterfaceiv);
        ASSIGN("glGetProgramResourceIndex", getProgramResourceIndex);
        ASSIGN("glGetProgramResourceLocation", getProgramResourceLocation);
        ASSIGN("glGetProgramResourceLocationIndex", getProgramResourceLocationIndex);
        ASSIGN("glGetProgramResourceName", getProgramResourceName);
        ASSIGN("glGetProgramResourceiv", getProgramResourceiv);
        ASSIGN("glInvalidateBufferData", invalidateBufferData);
        ASSIGN("glInvalidateBufferSubData", invalidateBufferSubData);
        ASSIGN("glInvalidateFramebuffer", invalidateFramebuffer);
        ASSIGN("glInvalidateSubFramebuffer", invalidateSubFramebuffer);
        ASSIGN("glInvalidateTexImage", invalidateTexImage);
        ASSIGN("glInvalidateTexSubImage", invalidateTexSubImage);
        ASSIGN("glMultiDrawArraysIndirect", multiDrawArraysIndirect);
        ASSIGN("glMultiDrawElementsIndirect", multiDrawElementsIndirect);
        ASSIGN("glObjectLabel", objectLabel);
        ASSIGN("glObjectPtrLabel", objectPtrLabel);
        ASSIGN("glPopDebugGroup", popDebugGroup);
        ASSIGN("glPushDebugGroup", pushDebugGroup);
        ASSIGN("glShaderStorageBlockBinding", shaderStorageBlockBinding);
        ASSIGN("glTexBufferRange", texBufferRange);
        ASSIGN("glTexStorage2DMultisample", texStorage2DMultisample);
        ASSIGN("glTexStorage3DMultisample", texStorage3DMultisample);
        ASSIGN("glTextureView", textureView);
        ASSIGN("glVertexAttribBinding", vertexAttribBinding);
        ASSIGN("glVertexAttribFormat", vertexAttribFormat);
        ASSIGN("glVertexAttribIFormat", vertexAttribIFormat);
        ASSIGN("glVertexAttribLFormat", vertexAttribLFormat);
        ASSIGN("glVertexBindingDivisor", vertexBindingDivisor);
    }

    // 4.4
    if (isAtLeastGL(gl::Version(4, 4)))
    {
        ASSIGN("glBindBuffersBase", bindBuffersBase);
        ASSIGN("glBindBuffersRange", bindBuffersRange);
        ASSIGN("glBindImageTextures", bindImageTextures);
        ASSIGN("glBindSamplers", bindSamplers);
        ASSIGN("glBindTextures", bindTextures);
        ASSIGN("glBindVertexBuffers", bindVertexBuffers);
        ASSIGN("glBufferStorage", bufferStorage);
        ASSIGN("glClearTexImage", clearTexImage);
        ASSIGN("glClearTexSubImage", clearTexSubImage);
    }

    // 4.5
    if (isAtLeastGL(gl::Version(4, 5)))
    {
        ASSIGN("glBindTextureUnit", bindTextureUnit);
        ASSIGN("glBlitNamedFramebuffer", blitNamedFramebuffer);
        ASSIGN("glCheckNamedFramebufferStatus", checkNamedFramebufferStatus);
        ASSIGN("glClearNamedBufferData", clearNamedBufferData);
        ASSIGN("glClearNamedBufferSubData", clearNamedBufferSubData);
        ASSIGN("glClearNamedFramebufferfi", clearNamedFramebufferfi);
        ASSIGN("glClearNamedFramebufferfv", clearNamedFramebufferfv);
        ASSIGN("glClearNamedFramebufferiv", clearNamedFramebufferiv);
        ASSIGN("glClearNamedFramebufferuiv", clearNamedFramebufferuiv);
        ASSIGN("glClipControl", clipControl);
        ASSIGN("glCompressedTextureSubImage1D", compressedTextureSubImage1D);
        ASSIGN("glCompressedTextureSubImage2D", compressedTextureSubImage2D);
        ASSIGN("glCompressedTextureSubImage3D", compressedTextureSubImage3D);
        ASSIGN("glCopyNamedBufferSubData", copyNamedBufferSubData);
        ASSIGN("glCopyTextureSubImage1D", copyTextureSubImage1D);
        ASSIGN("glCopyTextureSubImage2D", copyTextureSubImage2D);
        ASSIGN("glCopyTextureSubImage3D", copyTextureSubImage3D);
        ASSIGN("glCreateBuffers", createBuffers);
        ASSIGN("glCreateFramebuffers", createFramebuffers);
        ASSIGN("glCreateProgramPipelines", createProgramPipelines);
        ASSIGN("glCreateQueries", createQueries);
        ASSIGN("glCreateRenderbuffers", createRenderbuffers);
        ASSIGN("glCreateSamplers", createSamplers);
        ASSIGN("glCreateTextures", createTextures);
        ASSIGN("glCreateTransformFeedbacks", createTransformFeedbacks);
        ASSIGN("glCreateVertexArrays", createVertexArrays);
        ASSIGN("glDisableVertexArrayAttrib", disableVertexArrayAttrib);
        ASSIGN("glEnableVertexArrayAttrib", enableVertexArrayAttrib);
        ASSIGN("glFlushMappedNamedBufferRange", flushMappedNamedBufferRange);
        ASSIGN("glGenerateTextureMipmap", generateTextureMipmap);
        ASSIGN("glGetCompressedTextureImage", getCompressedTextureImage);
        ASSIGN("glGetCompressedTextureSubImage", getCompressedTextureSubImage);
        ASSIGN("glGetGraphicsResetStatus", getGraphicsResetStatus);
        ASSIGN("glGetNamedBufferParameteri64v", getNamedBufferParameteri64v);
        ASSIGN("glGetNamedBufferParameteriv", getNamedBufferParameteriv);
        ASSIGN("glGetNamedBufferPointerv", getNamedBufferPointerv);
        ASSIGN("glGetNamedBufferSubData", getNamedBufferSubData);
        ASSIGN("glGetNamedFramebufferAttachmentParameteriv", getNamedFramebufferAttachmentParameteriv);
        ASSIGN("glGetNamedFramebufferParameteriv", getNamedFramebufferParameteriv);
        ASSIGN("glGetNamedRenderbufferParameteriv", getNamedRenderbufferParameteriv);
        ASSIGN("glGetQueryBufferObjecti64v", getQueryBufferObjecti64v);
        ASSIGN("glGetQueryBufferObjectiv", getQueryBufferObjectiv);
        ASSIGN("glGetQueryBufferObjectui64v", getQueryBufferObjectui64v);
        ASSIGN("glGetQueryBufferObjectuiv", getQueryBufferObjectuiv);
        ASSIGN("glGetTextureImage", getTextureImage);
        ASSIGN("glGetTextureLevelParameterfv", getTextureLevelParameterfv);
        ASSIGN("glGetTextureLevelParameteriv", getTextureLevelParameteriv);
        ASSIGN("glGetTextureParameterIiv", getTextureParameterIiv);
        ASSIGN("glGetTextureParameterIuiv", getTextureParameterIuiv);
        ASSIGN("glGetTextureParameterfv", getTextureParameterfv);
        ASSIGN("glGetTextureParameteriv", getTextureParameteriv);
        ASSIGN("glGetTextureSubImage", getTextureSubImage);
        ASSIGN("glGetTransformFeedbacki64_v", getTransformFeedbacki64_v);
        ASSIGN("glGetTransformFeedbacki_v", getTransformFeedbacki_v);
        ASSIGN("glGetTransformFeedbackiv", getTransformFeedbackiv);
        ASSIGN("glGetVertexArrayIndexed64iv", getVertexArrayIndexed64iv);
        ASSIGN("glGetVertexArrayIndexediv", getVertexArrayIndexediv);
        ASSIGN("glGetVertexArrayiv", getVertexArrayiv);
        ASSIGN("glGetnCompressedTexImage", getnCompressedTexImage);
        ASSIGN("glGetnTexImage", getnTexImage);
        ASSIGN("glGetnUniformdv", getnUniformdv);
        ASSIGN("glGetnUniformfv", getnUniformfv);
        ASSIGN("glGetnUniformiv", getnUniformiv);
        ASSIGN("glGetnUniformuiv", getnUniformuiv);
        ASSIGN("glInvalidateNamedFramebufferData", invalidateNamedFramebufferData);
        ASSIGN("glInvalidateNamedFramebufferSubData", invalidateNamedFramebufferSubData);
        ASSIGN("glMapNamedBuffer", mapNamedBuffer);
        ASSIGN("glMapNamedBufferRange", mapNamedBufferRange);
        ASSIGN("glMemoryBarrierByRegion", memoryBarrierByRegion);
        ASSIGN("glNamedBufferData", namedBufferData);
        ASSIGN("glNamedBufferStorage", namedBufferStorage);
        ASSIGN("glNamedBufferSubData", namedBufferSubData);
        ASSIGN("glNamedFramebufferDrawBuffer", namedFramebufferDrawBuffer);
        ASSIGN("glNamedFramebufferDrawBuffers", namedFramebufferDrawBuffers);
        ASSIGN("glNamedFramebufferParameteri", namedFramebufferParameteri);
        ASSIGN("glNamedFramebufferReadBuffer", namedFramebufferReadBuffer);
        ASSIGN("glNamedFramebufferRenderbuffer", namedFramebufferRenderbuffer);
        ASSIGN("glNamedFramebufferTexture", namedFramebufferTexture);
        ASSIGN("glNamedFramebufferTextureLayer", namedFramebufferTextureLayer);
        ASSIGN("glNamedRenderbufferStorage", namedRenderbufferStorage);
        ASSIGN("glNamedRenderbufferStorageMultisample", namedRenderbufferStorageMultisample);
        ASSIGN("glReadnPixels", readnPixels);
        ASSIGN("glTextureBarrier", textureBarrier);
        ASSIGN("glTextureBuffer", textureBuffer);
        ASSIGN("glTextureBufferRange", textureBufferRange);
        ASSIGN("glTextureParameterIiv", textureParameterIiv);
        ASSIGN("glTextureParameterIuiv", textureParameterIuiv);
        ASSIGN("glTextureParameterf", textureParameterf);
        ASSIGN("glTextureParameterfv", textureParameterfv);
        ASSIGN("glTextureParameteri", textureParameteri);
        ASSIGN("glTextureParameteriv", textureParameteriv);
        ASSIGN("glTextureStorage1D", textureStorage1D);
        ASSIGN("glTextureStorage2D", textureStorage2D);
        ASSIGN("glTextureStorage2DMultisample", textureStorage2DMultisample);
        ASSIGN("glTextureStorage3D", textureStorage3D);
        ASSIGN("glTextureStorage3DMultisample", textureStorage3DMultisample);
        ASSIGN("glTextureSubImage1D", textureSubImage1D);
        ASSIGN("glTextureSubImage2D", textureSubImage2D);
        ASSIGN("glTextureSubImage3D", textureSubImage3D);
        ASSIGN("glTransformFeedbackBufferBase", transformFeedbackBufferBase);
        ASSIGN("glTransformFeedbackBufferRange", transformFeedbackBufferRange);
        ASSIGN("glUnmapNamedBuffer", unmapNamedBuffer);
        ASSIGN("glVertexArrayAttribBinding", vertexArrayAttribBinding);
        ASSIGN("glVertexArrayAttribFormat", vertexArrayAttribFormat);
        ASSIGN("glVertexArrayAttribIFormat", vertexArrayAttribIFormat);
        ASSIGN("glVertexArrayAttribLFormat", vertexArrayAttribLFormat);
        ASSIGN("glVertexArrayBindingDivisor", vertexArrayBindingDivisor);
        ASSIGN("glVertexArrayElementBuffer", vertexArrayElementBuffer);
        ASSIGN("glVertexArrayVertexBuffer", vertexArrayVertexBuffer);
        ASSIGN("glVertexArrayVertexBuffers", vertexArrayVertexBuffers);
    }

    // clang-format on
}

void FunctionsGL::initializeProcsGLES()
{
    // No profiles in GLES
    profile = 0;

    // clang-format off

    // GL_NV_internalformat_sample_query
    ASSIGN_WITH_EXT("GL_NV_internalformat_sample_query", "glGetInternalformatSampleivNV", getInternalformatSampleivNV);

    // GL_NV_path_rendering
    ASSIGN_WITH_EXT("GL_NV_path_rendering", "glMatrixLoadfEXT", matrixLoadEXT);
    ASSIGN_WITH_EXT("GL_NV_path_rendering", "glGenPathsNV", genPathsNV);
    ASSIGN_WITH_EXT("GL_NV_path_rendering", "glDeletePathsNV", delPathsNV);
    ASSIGN_WITH_EXT("GL_NV_path_rendering", "glPathCommandsNV", pathCommandsNV);
    ASSIGN_WITH_EXT("GL_NV_path_rendering", "glIsPathNV", isPathNV);
    ASSIGN_WITH_EXT("GL_NV_path_rendering", "glPathParameterfNV", setPathParameterfNV);
    ASSIGN_WITH_EXT("GL_NV_path_rendering", "glPathParameteriNV", setPathParameteriNV);
    ASSIGN_WITH_EXT("GL_NV_path_rendering", "glGetPathParameterfvNV", getPathParameterfNV);
    ASSIGN_WITH_EXT("GL_NV_path_rendering", "glGetPathParameterivNV", getPathParameteriNV);
    ASSIGN_WITH_EXT("GL_NV_path_rendering", "glPathStencilFuncNV", pathStencilFuncNV);
    ASSIGN_WITH_EXT("GL_NV_path_rendering", "glStencilFillPathNV", stencilFillPathNV);
    ASSIGN_WITH_EXT("GL_NV_path_rendering", "glStencilStrokePathNV", stencilStrokePathNV);
    ASSIGN_WITH_EXT("GL_NV_path_rendering", "glCoverFillPathNV", coverFillPathNV);
    ASSIGN_WITH_EXT("GL_NV_path_rendering", "glCoverStrokePathNV", coverStrokePathNV);
    ASSIGN_WITH_EXT("GL_NV_path_rendering", "glStencilThenCoverFillPathNV", stencilThenCoverFillPathNV);
    ASSIGN_WITH_EXT("GL_NV_path_rendering", "glStencilThenCoverStrokePathNV", stencilThenCoverStrokePathNV);
    ASSIGN_WITH_EXT("GL_NV_path_rendering", "glCoverFillPathInstancedNV", coverFillPathInstancedNV);
    ASSIGN_WITH_EXT("GL_NV_path_rendering", "glCoverStrokePathInstancedNV", coverStrokePathInstancedNV);
    ASSIGN_WITH_EXT("GL_NV_path_rendering", "glStencilFillPathInstancedNV", stencilFillPathInstancedNV);
    ASSIGN_WITH_EXT("GL_NV_path_rendering", "glStencilStrokePathInstancedNV", stencilStrokePathInstancedNV);
    ASSIGN_WITH_EXT("GL_NV_path_rendering", "glStencilThenCoverFillPathInstancedNV", stencilThenCoverFillPathInstancedNV);
    ASSIGN_WITH_EXT("GL_NV_path_rendering", "glStencilThenCoverStrokePathInstancedNV", stencilThenCoverStrokePathInstancedNV);
    ASSIGN_WITH_EXT("GL_NV_path_rendering", "glProgramPathFragmentInputGenNV", programPathFragmentInputGenNV);

    // GL_OES_texture_3D
    ASSIGN_WITH_EXT("GL_OES_texture_3D", "glTexImage3DOES", texImage3D);
    ASSIGN_WITH_EXT("GL_OES_texture_3D", "glTexSubImage3DOES", texSubImage3D);
    ASSIGN_WITH_EXT("GL_OES_texture_3D", "glCopyTexSubImage3DOES", copyTexSubImage3D);

    // GL_NV_framebuffer_mixed_samples
    ASSIGN_WITH_EXT("GL_NV_framebuffer_mixed_samples", "glCoverageModulationNV", coverageModulationNV);

    // GL_NV_fence
    ASSIGN_WITH_EXT("GL_NV_fence", "glDeleteFencesNV", deleteFencesNV);
    ASSIGN_WITH_EXT("GL_NV_fence", "glGenFencesNV", genFencesNV);
    ASSIGN_WITH_EXT("GL_NV_fence", "glIsFenceNV", isFenceNV);
    ASSIGN_WITH_EXT("GL_NV_fence", "glTestFenceNV", testFenceNV);
    ASSIGN_WITH_EXT("GL_NV_fence", "glGetFenceivNV", getFenceivNV);
    ASSIGN_WITH_EXT("GL_NV_fence", "glFinishFenceNV", finishFenceNV);
    ASSIGN_WITH_EXT("GL_NV_fence", "glSetFenceNV", setFenceNV);

    // GL_EXT_texture_storage
    ASSIGN_WITH_EXT("GL_EXT_texture_storage", "glTexStorage2DEXT", texStorage2D);
    ASSIGN_WITH_EXT("GL_EXT_texture_storage GL_OES_texture3D", "glTexStorage3DEXT", texStorage3D);

    // GL_OES_vertex_array_object
    ASSIGN_WITH_EXT("GL_OES_vertex_array_object", "glBindVertexArray", bindVertexArray);
    ASSIGN_WITH_EXT("GL_OES_vertex_array_object", "glDeleteVertexArrays", deleteVertexArrays);
    ASSIGN_WITH_EXT("GL_OES_vertex_array_object", "glGenVertexArrays", genVertexArrays);
    ASSIGN_WITH_EXT("GL_OES_vertex_array_object", "glIsVertexArray", isVertexArray);

    // GL_EXT_map_buffer_range
    ASSIGN_WITH_EXT("GL_EXT_map_buffer_range", "glMapBufferRangeEXT", mapBufferRange);
    ASSIGN_WITH_EXT("GL_EXT_map_buffer_range", "glFlushMappedBufferRangeEXT", flushMappedBufferRange);
    ASSIGN_WITH_EXT("GL_EXT_map_buffer_range", "glUnmapBufferOES", unmapBuffer);

    // GL_OES_mapbuffer
    ASSIGN_WITH_EXT("GL_OES_mapbuffer", "glMapBufferOES", mapBuffer);
    ASSIGN_WITH_EXT("GL_OES_mapbuffer", "glUnmapBufferOES", unmapBuffer);

    // GL_KHR_debug
    ASSIGN_WITH_EXT("GL_KHR_debug", "glDebugMessageControlKHR", debugMessageControl);
    ASSIGN_WITH_EXT("GL_KHR_debug", "glDebugMessageInsertKHR", debugMessageInsert);
    ASSIGN_WITH_EXT("GL_KHR_debug", "glDebugMessageCallbackKHR", debugMessageCallback);
    ASSIGN_WITH_EXT("GL_KHR_debug", "glGetDebugMessageLogKHR", getDebugMessageLog);
    ASSIGN_WITH_EXT("GL_KHR_debug", "glGetPointervKHR", getPointerv);
    ASSIGN_WITH_EXT("GL_KHR_debug", "glPushDebugGroupKHR", pushDebugGroup);
    ASSIGN_WITH_EXT("GL_KHR_debug", "glPopDebugGroupKHR", popDebugGroup);
    ASSIGN_WITH_EXT("GL_KHR_debug", "glObjectLabelKHR", objectLabel);
    ASSIGN_WITH_EXT("GL_KHR_debug", "glGetObjectLabelKHR", getObjectLabel);
    ASSIGN_WITH_EXT("GL_KHR_debug", "glObjectPtrLabelKHR", objectPtrLabel);
    ASSIGN_WITH_EXT("GL_KHR_debug", "glGetObjectPtrLabelKHR", getObjectPtrLabel);

    // GL_EXT_draw_instanced
    ASSIGN_WITH_EXT("GL_EXT_draw_instanced", "glVertexAttribDivisorEXT", vertexAttribDivisor);
    ASSIGN_WITH_EXT("GL_EXT_draw_instanced", "glDrawArraysInstancedEXT", drawArraysInstanced);
    ASSIGN_WITH_EXT("GL_EXT_draw_instanced", "glDrawElementsInstancedEXT", drawElementsInstanced);

    // GL_EXT_occlusion_query_boolean
    ASSIGN_WITH_EXT("GL_EXT_occlusion_query_boolean", "glGenQueriesEXT", genQueries);
    ASSIGN_WITH_EXT("GL_EXT_occlusion_query_boolean", "glDeleteQueriesEXT", deleteQueries);
    ASSIGN_WITH_EXT("GL_EXT_occlusion_query_boolean", "glIsQueryEXT", isQuery);
    ASSIGN_WITH_EXT("GL_EXT_occlusion_query_boolean", "glBeginQueryEXT", beginQuery);
    ASSIGN_WITH_EXT("GL_EXT_occlusion_query_boolean", "glEndQueryEXT", endQuery);
    ASSIGN_WITH_EXT("GL_EXT_occlusion_query_boolean", "glGetQueryivEXT", getQueryiv);
    ASSIGN_WITH_EXT("GL_EXT_occlusion_query_boolean", "glGetQueryObjectuivEXT", getQueryObjectuiv);

    // GL_EXT_disjoint_timer_query
    ASSIGN_WITH_EXT("GL_EXT_disjoint_timer_query", "glGenQueriesEXT", genQueries);
    ASSIGN_WITH_EXT("GL_EXT_disjoint_timer_query", "glDeleteQueriesEXT", deleteQueries);
    ASSIGN_WITH_EXT("GL_EXT_disjoint_timer_query", "glIsQueryEXT", isQuery);
    ASSIGN_WITH_EXT("GL_EXT_disjoint_timer_query", "glBeginQueryEXT", beginQuery);
    ASSIGN_WITH_EXT("GL_EXT_disjoint_timer_query", "glEndQueryEXT", endQuery);
    ASSIGN_WITH_EXT("GL_EXT_disjoint_timer_query", "glQueryCounterEXT", queryCounter);
    ASSIGN_WITH_EXT("GL_EXT_disjoint_timer_query", "glGetQueryivEXT", getQueryiv);
    ASSIGN_WITH_EXT("GL_EXT_disjoint_timer_query", "glGetQueryObjectivEXT", getQueryObjectiv);
    ASSIGN_WITH_EXT("GL_EXT_disjoint_timer_query", "glGetQueryObjectuivEXT", getQueryObjectuiv);
    ASSIGN_WITH_EXT("GL_EXT_disjoint_timer_query", "glGetQueryObjecti64vEXT", getQueryObjecti64v);
    ASSIGN_WITH_EXT("GL_EXT_disjoint_timer_query", "glGetQueryObjectui64vEXT", getQueryObjectui64v);

    // GL_OES_EGL_image
    ASSIGN_WITH_EXT("GL_OES_EGL_image", "glEGLImageTargetRenderbufferStorageOES", eglImageTargetRenderbufferStorageOES);
    ASSIGN_WITH_EXT("GL_OES_EGL_image", "glEGLImageTargetTexture2DOES", eglImageTargetTexture2DOES);

    // GL_OES_get_program_binary
    ASSIGN_WITH_EXT("GL_OES_get_program_binary", "glGetProgramBinaryOES", getProgramBinary);
    ASSIGN_WITH_EXT("GL_OES_get_program_binary", "glProgramBinaryOES", programBinary);

    // GL_EXT_robustness
    ASSIGN_WITH_EXT("GL_EXT_robustness", "glGetGraphicsResetStatusEXT", getGraphicsResetStatus);

    // GL_KHR_robustness
    ASSIGN_WITH_EXT("GL_KHR_robustness", "glGetGraphicsResetStatusKHR", getGraphicsResetStatus);

    // GL_EXT_discard_framebuffer
    ASSIGN_WITH_EXT("GL_EXT_discard_framebuffer", "glDiscardFramebufferEXT", discardFramebuffer);

    // 2.0
    if (isAtLeastGLES(gl::Version(2, 0)))
    {
        ASSIGN("glActiveTexture", activeTexture);
        ASSIGN("glAttachShader", attachShader);
        ASSIGN("glBindAttribLocation", bindAttribLocation);
        ASSIGN("glBindBuffer", bindBuffer);
        ASSIGN("glBindFramebuffer", bindFramebuffer);
        ASSIGN("glBindRenderbuffer", bindRenderbuffer);
        ASSIGN("glBindTexture", bindTexture);
        ASSIGN("glBlendColor", blendColor);
        ASSIGN("glBlendEquation", blendEquation);
        ASSIGN("glBlendEquationSeparate", blendEquationSeparate);
        ASSIGN("glBlendFunc", blendFunc);
        ASSIGN("glBlendFuncSeparate", blendFuncSeparate);
        ASSIGN("glBufferData", bufferData);
        ASSIGN("glBufferSubData", bufferSubData);
        ASSIGN("glCheckFramebufferStatus", checkFramebufferStatus);
        ASSIGN("glClear", clear);
        ASSIGN("glClearColor", clearColor);
        ASSIGN("glClearDepthf", clearDepthf);
        ASSIGN("glClearStencil", clearStencil);
        ASSIGN("glColorMask", colorMask);
        ASSIGN("glCompileShader", compileShader);
        ASSIGN("glCompressedTexImage2D", compressedTexImage2D);
        ASSIGN("glCompressedTexSubImage2D", compressedTexSubImage2D);
        ASSIGN("glCopyTexImage2D", copyTexImage2D);
        ASSIGN("glCopyTexSubImage2D", copyTexSubImage2D);
        ASSIGN("glCreateProgram", createProgram);
        ASSIGN("glCreateShader", createShader);
        ASSIGN("glCullFace", cullFace);
        ASSIGN("glDeleteBuffers", deleteBuffers);
        ASSIGN("glDeleteFramebuffers", deleteFramebuffers);
        ASSIGN("glDeleteProgram", deleteProgram);
        ASSIGN("glDeleteRenderbuffers", deleteRenderbuffers);
        ASSIGN("glDeleteShader", deleteShader);
        ASSIGN("glDeleteTextures", deleteTextures);
        ASSIGN("glDepthFunc", depthFunc);
        ASSIGN("glDepthMask", depthMask);
        ASSIGN("glDepthRangef", depthRangef);
        ASSIGN("glDetachShader", detachShader);
        ASSIGN("glDisable", disable);
        ASSIGN("glDisableVertexAttribArray", disableVertexAttribArray);
        ASSIGN("glDrawArrays", drawArrays);
        ASSIGN("glDrawElements", drawElements);
        ASSIGN("glEnable", enable);
        ASSIGN("glEnableVertexAttribArray", enableVertexAttribArray);
        ASSIGN("glFinish", finish);
        ASSIGN("glFlush", flush);
        ASSIGN("glFramebufferRenderbuffer", framebufferRenderbuffer);
        ASSIGN("glFramebufferTexture2D", framebufferTexture2D);
        ASSIGN("glFrontFace", frontFace);
        ASSIGN("glGenBuffers", genBuffers);
        ASSIGN("glGenerateMipmap", generateMipmap);
        ASSIGN("glGenFramebuffers", genFramebuffers);
        ASSIGN("glGenRenderbuffers", genRenderbuffers);
        ASSIGN("glGenTextures", genTextures);
        ASSIGN("glGetActiveAttrib", getActiveAttrib);
        ASSIGN("glGetActiveUniform", getActiveUniform);
        ASSIGN("glGetAttachedShaders", getAttachedShaders);
        ASSIGN("glGetAttribLocation", getAttribLocation);
        ASSIGN("glGetBooleanv", getBooleanv);
        ASSIGN("glGetBufferParameteriv", getBufferParameteriv);
        ASSIGN("glGetError", getError);
        ASSIGN("glGetFloatv", getFloatv);
        ASSIGN("glGetFramebufferAttachmentParameteriv", getFramebufferAttachmentParameteriv);
        ASSIGN("glGetIntegerv", getIntegerv);
        ASSIGN("glGetProgramiv", getProgramiv);
        ASSIGN("glGetProgramInfoLog", getProgramInfoLog);
        ASSIGN("glGetRenderbufferParameteriv", getRenderbufferParameteriv);
        ASSIGN("glGetShaderiv", getShaderiv);
        ASSIGN("glGetShaderInfoLog", getShaderInfoLog);
        ASSIGN("glGetShaderPrecisionFormat", getShaderPrecisionFormat);
        ASSIGN("glGetShaderSource", getShaderSource);
        ASSIGN("glGetString", getString);
        ASSIGN("glGetTexParameterfv", getTexParameterfv);
        ASSIGN("glGetTexParameteriv", getTexParameteriv);
        ASSIGN("glGetUniformfv", getUniformfv);
        ASSIGN("glGetUniformiv", getUniformiv);
        ASSIGN("glGetUniformLocation", getUniformLocation);
        ASSIGN("glGetVertexAttribfv", getVertexAttribfv);
        ASSIGN("glGetVertexAttribiv", getVertexAttribiv);
        ASSIGN("glGetVertexAttribPointerv", getVertexAttribPointerv);
        ASSIGN("glHint", hint);
        ASSIGN("glIsBuffer", isBuffer);
        ASSIGN("glIsEnabled", isEnabled);
        ASSIGN("glIsFramebuffer", isFramebuffer);
        ASSIGN("glIsProgram", isProgram);
        ASSIGN("glIsRenderbuffer", isRenderbuffer);
        ASSIGN("glIsShader", isShader);
        ASSIGN("glIsTexture", isTexture);
        ASSIGN("glLineWidth", lineWidth);
        ASSIGN("glLinkProgram", linkProgram);
        ASSIGN("glPixelStorei", pixelStorei);
        ASSIGN("glPolygonOffset", polygonOffset);
        ASSIGN("glReadPixels", readPixels);
        ASSIGN("glReleaseShaderCompiler", releaseShaderCompiler);
        ASSIGN("glRenderbufferStorage", renderbufferStorage);
        ASSIGN("glSampleCoverage", sampleCoverage);
        ASSIGN("glScissor", scissor);
        ASSIGN("glShaderBinary", shaderBinary);
        ASSIGN("glShaderSource", shaderSource);
        ASSIGN("glStencilFunc", stencilFunc);
        ASSIGN("glStencilFuncSeparate", stencilFuncSeparate);
        ASSIGN("glStencilMask", stencilMask);
        ASSIGN("glStencilMaskSeparate", stencilMaskSeparate);
        ASSIGN("glStencilOp", stencilOp);
        ASSIGN("glStencilOpSeparate", stencilOpSeparate);
        ASSIGN("glTexImage2D", texImage2D);
        ASSIGN("glTexParameterf", texParameterf);
        ASSIGN("glTexParameterfv", texParameterfv);
        ASSIGN("glTexParameteri", texParameteri);
        ASSIGN("glTexParameteriv", texParameteriv);
        ASSIGN("glTexSubImage2D", texSubImage2D);
        ASSIGN("glUniform1f", uniform1f);
        ASSIGN("glUniform1fv", uniform1fv);
        ASSIGN("glUniform1i", uniform1i);
        ASSIGN("glUniform1iv", uniform1iv);
        ASSIGN("glUniform2f", uniform2f);
        ASSIGN("glUniform2fv", uniform2fv);
        ASSIGN("glUniform2i", uniform2i);
        ASSIGN("glUniform2iv", uniform2iv);
        ASSIGN("glUniform3f", uniform3f);
        ASSIGN("glUniform3fv", uniform3fv);
        ASSIGN("glUniform3i", uniform3i);
        ASSIGN("glUniform3iv", uniform3iv);
        ASSIGN("glUniform4f", uniform4f);
        ASSIGN("glUniform4fv", uniform4fv);
        ASSIGN("glUniform4i", uniform4i);
        ASSIGN("glUniform4iv", uniform4iv);
        ASSIGN("glUniformMatrix2fv", uniformMatrix2fv);
        ASSIGN("glUniformMatrix3fv", uniformMatrix3fv);
        ASSIGN("glUniformMatrix4fv", uniformMatrix4fv);
        ASSIGN("glUseProgram", useProgram);
        ASSIGN("glValidateProgram", validateProgram);
        ASSIGN("glVertexAttrib1f", vertexAttrib1f);
        ASSIGN("glVertexAttrib1fv", vertexAttrib1fv);
        ASSIGN("glVertexAttrib2f", vertexAttrib2f);
        ASSIGN("glVertexAttrib2fv", vertexAttrib2fv);
        ASSIGN("glVertexAttrib3f", vertexAttrib3f);
        ASSIGN("glVertexAttrib3fv", vertexAttrib3fv);
        ASSIGN("glVertexAttrib4f", vertexAttrib4f);
        ASSIGN("glVertexAttrib4fv", vertexAttrib4fv);
        ASSIGN("glVertexAttribPointer", vertexAttribPointer);
        ASSIGN("glViewport", viewport);
    }

    // 3.0
    if (isAtLeastGLES(gl::Version(3, 0)))
    {
        ASSIGN("glReadBuffer", readBuffer);
        ASSIGN("glDrawRangeElements", drawRangeElements);
        ASSIGN("glTexImage3D", texImage3D);
        ASSIGN("glTexSubImage3D", texSubImage3D);
        ASSIGN("glCopyTexSubImage3D", copyTexSubImage3D);
        ASSIGN("glCompressedTexImage3D", compressedTexImage3D);
        ASSIGN("glCompressedTexSubImage3D", compressedTexSubImage3D);
        ASSIGN("glGenQueries", genQueries);
        ASSIGN("glDeleteQueries", deleteQueries);
        ASSIGN("glIsQuery", isQuery);
        ASSIGN("glBeginQuery", beginQuery);
        ASSIGN("glEndQuery", endQuery);
        ASSIGN("glGetQueryiv", getQueryiv);
        ASSIGN("glGetQueryObjectuiv", getQueryObjectuiv);
        ASSIGN("glUnmapBuffer", unmapBuffer);
        ASSIGN("glGetBufferPointerv", getBufferPointerv);
        ASSIGN("glDrawBuffers", drawBuffers);
        ASSIGN("glUniformMatrix2x3fv", uniformMatrix2x3fv);
        ASSIGN("glUniformMatrix3x2fv", uniformMatrix3x2fv);
        ASSIGN("glUniformMatrix2x4fv", uniformMatrix2x4fv);
        ASSIGN("glUniformMatrix4x2fv", uniformMatrix4x2fv);
        ASSIGN("glUniformMatrix3x4fv", uniformMatrix3x4fv);
        ASSIGN("glUniformMatrix4x3fv", uniformMatrix4x3fv);
        ASSIGN("glBlitFramebuffer", blitFramebuffer);
        ASSIGN("glRenderbufferStorageMultisample", renderbufferStorageMultisample);
        ASSIGN("glFramebufferTextureLayer", framebufferTextureLayer);
        ASSIGN("glMapBufferRange", mapBufferRange);
        ASSIGN("glFlushMappedBufferRange", flushMappedBufferRange);
        ASSIGN("glBindVertexArray", bindVertexArray);
        ASSIGN("glDeleteVertexArrays", deleteVertexArrays);
        ASSIGN("glGenVertexArrays", genVertexArrays);
        ASSIGN("glIsVertexArray", isVertexArray);
        ASSIGN("glGetIntegeri_v", getIntegeri_v);
        ASSIGN("glBeginTransformFeedback", beginTransformFeedback);
        ASSIGN("glEndTransformFeedback", endTransformFeedback);
        ASSIGN("glBindBufferRange", bindBufferRange);
        ASSIGN("glBindBufferBase", bindBufferBase);
        ASSIGN("glTransformFeedbackVaryings", transformFeedbackVaryings);
        ASSIGN("glGetTransformFeedbackVarying", getTransformFeedbackVarying);
        ASSIGN("glVertexAttribIPointer", vertexAttribIPointer);
        ASSIGN("glGetVertexAttribIiv", getVertexAttribIiv);
        ASSIGN("glGetVertexAttribIuiv", getVertexAttribIuiv);
        ASSIGN("glVertexAttribI4i", vertexAttribI4i);
        ASSIGN("glVertexAttribI4ui", vertexAttribI4ui);
        ASSIGN("glVertexAttribI4iv", vertexAttribI4iv);
        ASSIGN("glVertexAttribI4uiv", vertexAttribI4uiv);
        ASSIGN("glGetUniformuiv", getUniformuiv);
        ASSIGN("glGetFragDataLocation", getFragDataLocation);
        ASSIGN("glUniform1ui", uniform1ui);
        ASSIGN("glUniform2ui", uniform2ui);
        ASSIGN("glUniform3ui", uniform3ui);
        ASSIGN("glUniform4ui", uniform4ui);
        ASSIGN("glUniform1uiv", uniform1uiv);
        ASSIGN("glUniform2uiv", uniform2uiv);
        ASSIGN("glUniform3uiv", uniform3uiv);
        ASSIGN("glUniform4uiv", uniform4uiv);
        ASSIGN("glClearBufferiv", clearBufferiv);
        ASSIGN("glClearBufferuiv", clearBufferuiv);
        ASSIGN("glClearBufferfv", clearBufferfv);
        ASSIGN("glClearBufferfi", clearBufferfi);
        ASSIGN("glGetStringi", getStringi);
        ASSIGN("glCopyBufferSubData", copyBufferSubData);
        ASSIGN("glGetUniformIndices", getUniformIndices);
        ASSIGN("glGetActiveUniformsiv", getActiveUniformsiv);
        ASSIGN("glGetUniformBlockIndex", getUniformBlockIndex);
        ASSIGN("glGetActiveUniformBlockiv", getActiveUniformBlockiv);
        ASSIGN("glGetActiveUniformBlockName", getActiveUniformBlockName);
        ASSIGN("glUniformBlockBinding", uniformBlockBinding);
        ASSIGN("glDrawArraysInstanced", drawArraysInstanced);
        ASSIGN("glDrawElementsInstanced", drawElementsInstanced);
        ASSIGN("glFenceSync", fenceSync);
        ASSIGN("glIsSync", isSync);
        ASSIGN("glDeleteSync", deleteSync);
        ASSIGN("glClientWaitSync", clientWaitSync);
        ASSIGN("glWaitSync", waitSync);
        ASSIGN("glGetInteger64v", getInteger64v);
        ASSIGN("glGetSynciv", getSynciv);
        ASSIGN("glGetInteger64i_v", getInteger64i_v);
        ASSIGN("glGetBufferParameteri64v", getBufferParameteri64v);
        ASSIGN("glGenSamplers", genSamplers);
        ASSIGN("glDeleteSamplers", deleteSamplers);
        ASSIGN("glIsSampler", isSampler);
        ASSIGN("glBindSampler", bindSampler);
        ASSIGN("glSamplerParameteri", samplerParameteri);
        ASSIGN("glSamplerParameteriv", samplerParameteriv);
        ASSIGN("glSamplerParameterf", samplerParameterf);
        ASSIGN("glSamplerParameterfv", samplerParameterfv);
        ASSIGN("glGetSamplerParameteriv", getSamplerParameteriv);
        ASSIGN("glGetSamplerParameterfv", getSamplerParameterfv);
        ASSIGN("glVertexAttribDivisor", vertexAttribDivisor);
        ASSIGN("glBindTransformFeedback", bindTransformFeedback);
        ASSIGN("glDeleteTransformFeedbacks", deleteTransformFeedbacks);
        ASSIGN("glGenTransformFeedbacks", genTransformFeedbacks);
        ASSIGN("glIsTransformFeedback", isTransformFeedback);
        ASSIGN("glPauseTransformFeedback", pauseTransformFeedback);
        ASSIGN("glResumeTransformFeedback", resumeTransformFeedback);
        ASSIGN("glGetProgramBinary", getProgramBinary);
        ASSIGN("glProgramBinary", programBinary);
        ASSIGN("glProgramParameteri", programParameteri);
        ASSIGN("glInvalidateFramebuffer", invalidateFramebuffer);
        ASSIGN("glInvalidateSubFramebuffer", invalidateSubFramebuffer);
        ASSIGN("glTexStorage2D", texStorage2D);
        ASSIGN("glTexStorage3D", texStorage3D);
        ASSIGN("glGetInternalformativ", getInternalformativ);
    }

    // 3.1
    if (isAtLeastGLES(gl::Version(3, 1)))
    {
        ASSIGN("glDispatchCompute", dispatchCompute);
        ASSIGN("glDispatchComputeIndirect", dispatchComputeIndirect);
        ASSIGN("glDrawArraysIndirect", drawArraysIndirect);
        ASSIGN("glDrawElementsIndirect", drawElementsIndirect);
        ASSIGN("glFramebufferParameteri", framebufferParameteri);
        ASSIGN("glGetFramebufferParameteriv", getFramebufferParameteriv);
        ASSIGN("glGetProgramInterfaceiv", getProgramInterfaceiv);
        ASSIGN("glGetProgramResourceIndex", getProgramResourceIndex);
        ASSIGN("glGetProgramResourceName", getProgramResourceName);
        ASSIGN("glGetProgramResourceiv", getProgramResourceiv);
        ASSIGN("glGetProgramResourceLocation", getProgramResourceLocation);
        ASSIGN("glUseProgramStages", useProgramStages);
        ASSIGN("glActiveShaderProgram", activeShaderProgram);
        ASSIGN("glCreateShaderProgramv", createShaderProgramv);
        ASSIGN("glBindProgramPipeline", bindProgramPipeline);
        ASSIGN("glDeleteProgramPipelines", deleteProgramPipelines);
        ASSIGN("glGenProgramPipelines", genProgramPipelines);
        ASSIGN("glIsProgramPipeline", isProgramPipeline);
        ASSIGN("glGetProgramPipelineiv", getProgramPipelineiv);
        ASSIGN("glProgramUniform1i", programUniform1i);
        ASSIGN("glProgramUniform2i", programUniform2i);
        ASSIGN("glProgramUniform3i", programUniform3i);
        ASSIGN("glProgramUniform4i", programUniform4i);
        ASSIGN("glProgramUniform1ui", programUniform1ui);
        ASSIGN("glProgramUniform2ui", programUniform2ui);
        ASSIGN("glProgramUniform3ui", programUniform3ui);
        ASSIGN("glProgramUniform4ui", programUniform4ui);
        ASSIGN("glProgramUniform1f", programUniform1f);
        ASSIGN("glProgramUniform2f", programUniform2f);
        ASSIGN("glProgramUniform3f", programUniform3f);
        ASSIGN("glProgramUniform4f", programUniform4f);
        ASSIGN("glProgramUniform1iv", programUniform1iv);
        ASSIGN("glProgramUniform2iv", programUniform2iv);
        ASSIGN("glProgramUniform3iv", programUniform3iv);
        ASSIGN("glProgramUniform4iv", programUniform4iv);
        ASSIGN("glProgramUniform1uiv", programUniform1uiv);
        ASSIGN("glProgramUniform2uiv", programUniform2uiv);
        ASSIGN("glProgramUniform3uiv", programUniform3uiv);
        ASSIGN("glProgramUniform4uiv", programUniform4uiv);
        ASSIGN("glProgramUniform1fv", programUniform1fv);
        ASSIGN("glProgramUniform2fv", programUniform2fv);
        ASSIGN("glProgramUniform3fv", programUniform3fv);
        ASSIGN("glProgramUniform4fv", programUniform4fv);
        ASSIGN("glProgramUniformMatrix2fv", programUniformMatrix2fv);
        ASSIGN("glProgramUniformMatrix3fv", programUniformMatrix3fv);
        ASSIGN("glProgramUniformMatrix4fv", programUniformMatrix4fv);
        ASSIGN("glProgramUniformMatrix2x3fv", programUniformMatrix2x3fv);
        ASSIGN("glProgramUniformMatrix3x2fv", programUniformMatrix3x2fv);
        ASSIGN("glProgramUniformMatrix2x4fv", programUniformMatrix2x4fv);
        ASSIGN("glProgramUniformMatrix4x2fv", programUniformMatrix4x2fv);
        ASSIGN("glProgramUniformMatrix3x4fv", programUniformMatrix3x4fv);
        ASSIGN("glProgramUniformMatrix4x3fv", programUniformMatrix4x3fv);
        ASSIGN("glValidateProgramPipeline", validateProgramPipeline);
        ASSIGN("glGetProgramPipelineInfoLog", getProgramPipelineInfoLog);
        ASSIGN("glBindImageTexture", bindImageTexture);
        ASSIGN("glGetBooleani_v", getBooleani_v);
        ASSIGN("glMemoryBarrier", memoryBarrier);
        ASSIGN("glMemoryBarrierByRegion", memoryBarrierByRegion);
        ASSIGN("glTexStorage2DMultisample", texStorage2DMultisample);
        ASSIGN("glGetMultisamplefv", getMultisamplefv);
        ASSIGN("glSampleMaski", sampleMaski);
        ASSIGN("glGetTexLevelParameteriv", getTexLevelParameteriv);
        ASSIGN("glGetTexLevelParameterfv", getTexLevelParameterfv);
        ASSIGN("glBindVertexBuffer", bindVertexBuffer);
        ASSIGN("glVertexAttribFormat", vertexAttribFormat);
        ASSIGN("glVertexAttribIFormat", vertexAttribIFormat);
        ASSIGN("glVertexAttribBinding", vertexAttribBinding);
        ASSIGN("glVertexBindingDivisor", vertexBindingDivisor);
    }

    // 3.2
    if (isAtLeastGLES(gl::Version(3, 2)))
    {
        ASSIGN("glBlendBarrier", blendBarrier);
        ASSIGN("glCopyImageSubData", copyImageSubData);
        ASSIGN("glDebugMessageControl", debugMessageControl);
        ASSIGN("glDebugMessageInsert", debugMessageInsert);
        ASSIGN("glDebugMessageCallback", debugMessageCallback);
        ASSIGN("glGetDebugMessageLog", getDebugMessageLog);
        ASSIGN("glPushDebugGroup", pushDebugGroup);
        ASSIGN("glPopDebugGroup", popDebugGroup);
        ASSIGN("glObjectLabel", objectLabel);
        ASSIGN("glGetObjectLabel", getObjectLabel);
        ASSIGN("glObjectPtrLabel", objectPtrLabel);
        ASSIGN("glGetObjectPtrLabel", getObjectPtrLabel);
        ASSIGN("glGetPointerv", getPointerv);
        ASSIGN("glEnablei", enablei);
        ASSIGN("glDisablei", disablei);
        ASSIGN("glBlendEquationi", blendEquationi);
        ASSIGN("glBlendEquationSeparatei", blendEquationSeparatei);
        ASSIGN("glBlendFunci", blendFunci);
        ASSIGN("glBlendFuncSeparatei", blendFuncSeparatei);
        ASSIGN("glColorMaski", colorMaski);
        ASSIGN("glIsEnabledi", isEnabledi);
        ASSIGN("glDrawElementsBaseVertex", drawElementsBaseVertex);
        ASSIGN("glDrawRangeElementsBaseVertex", drawRangeElementsBaseVertex);
        ASSIGN("glDrawElementsInstancedBaseVertex", drawElementsInstancedBaseVertex);
        ASSIGN("glFramebufferTexture", framebufferTexture);
        ASSIGN("glPrimitiveBoundingBox", primitiveBoundingBox);
        ASSIGN("glGetGraphicsResetStatus", getGraphicsResetStatus);
        ASSIGN("glReadnPixels", readnPixels);
        ASSIGN("glGetnUniformfv", getnUniformfv);
        ASSIGN("glGetnUniformiv", getnUniformiv);
        ASSIGN("glGetnUniformuiv", getnUniformuiv);
        ASSIGN("glMinSampleShading", minSampleShading);
        ASSIGN("glPatchParameteri", patchParameteri);
        ASSIGN("glTexParameterIiv", texParameterIiv);
        ASSIGN("glTexParameterIuiv", texParameterIuiv);
        ASSIGN("glGetTexParameterIiv", getTexParameterIiv);
        ASSIGN("glGetTexParameterIuiv", getTexParameterIuiv);
        ASSIGN("glSamplerParameterIiv", samplerParameterIiv);
        ASSIGN("glSamplerParameterIuiv", samplerParameterIuiv);
        ASSIGN("glGetSamplerParameterIiv", getSamplerParameterIiv);
        ASSIGN("glGetSamplerParameterIuiv", getSamplerParameterIuiv);
        ASSIGN("glTexBuffer", texBuffer);
        ASSIGN("glTexBufferRange", texBufferRange);
        ASSIGN("glTexStorage3DMultisample", texStorage3DMultisample);
    }

    // clang-format on
}

bool FunctionsGL::isAtLeastGL(const gl::Version &glVersion) const
{
    return standard == STANDARD_GL_DESKTOP && version >= glVersion;
}

bool FunctionsGL::isAtMostGL(const gl::Version &glVersion) const
{
    return standard == STANDARD_GL_DESKTOP && glVersion >= version;
}

bool FunctionsGL::isAtLeastGLES(const gl::Version &glesVersion) const
{
    return standard == STANDARD_GL_ES && version >= glesVersion;
}

bool FunctionsGL::isAtMostGLES(const gl::Version &glesVersion) const
{
    return standard == STANDARD_GL_ES && glesVersion >= version;
}

bool FunctionsGL::hasExtension(const std::string &ext) const
{
    return std::find(extensions.begin(), extensions.end(), ext) != extensions.end();
}

bool FunctionsGL::hasGLExtension(const std::string &ext) const
{
    return standard == STANDARD_GL_DESKTOP && hasExtension(ext);
}

bool FunctionsGL::hasGLESExtension(const std::string &ext) const
{
    return standard == STANDARD_GL_ES && hasExtension(ext);
}

}  // namespace gl
