//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Context11:
//   D3D11-specific functionality associated with a GL Context.
//

#ifndef LIBANGLE_RENDERER_D3D_D3D11_CONTEXT11_H_
#define LIBANGLE_RENDERER_D3D_D3D11_CONTEXT11_H_

#include "libANGLE/renderer/ContextImpl.h"

#include "libANGLE/renderer/d3d/ContextD3D.h"

namespace rx
{
class Renderer11;

class Context11 : public ContextD3D, public MultisampleTextureInitializer
{
  public:
    Context11(const gl::ContextState &state, Renderer11 *renderer);
    ~Context11() override;

    gl::Error initialize() override;
    void onDestroy(const gl::Context *context) override;

    // Shader creation
    CompilerImpl *createCompiler() override;
    ShaderImpl *createShader(const gl::ShaderState &data) override;
    ProgramImpl *createProgram(const gl::ProgramState &data) override;

    // Framebuffer creation
    FramebufferImpl *createFramebuffer(const gl::FramebufferState &data) override;

    // Texture creation
    TextureImpl *createTexture(const gl::TextureState &state) override;

    // Renderbuffer creation
    RenderbufferImpl *createRenderbuffer(const gl::RenderbufferState &state) override;

    // Buffer creation
    BufferImpl *createBuffer(const gl::BufferState &state) override;

    // Vertex Array creation
    VertexArrayImpl *createVertexArray(const gl::VertexArrayState &data) override;

    // Query and Fence creation
    QueryImpl *createQuery(gl::QueryType type) override;
    FenceNVImpl *createFenceNV() override;
    SyncImpl *createSync() override;

    // Transform Feedback creation
    TransformFeedbackImpl *createTransformFeedback(
        const gl::TransformFeedbackState &state) override;

    // Sampler object creation
    SamplerImpl *createSampler(const gl::SamplerState &state) override;

    // Program Pipeline object creation
    ProgramPipelineImpl *createProgramPipeline(const gl::ProgramPipelineState &data) override;

    // Path object creation.
    std::vector<PathImpl *> createPaths(GLsizei) override;

    // Flush and finish.
    gl::Error flush(const gl::Context *context) override;
    gl::Error finish(const gl::Context *context) override;

    // Drawing methods.
    gl::Error drawArrays(const gl::Context *context,
                         gl::PrimitiveMode mode,
                         GLint first,
                         GLsizei count) override;
    gl::Error drawArraysInstanced(const gl::Context *context,
                                  gl::PrimitiveMode mode,
                                  GLint first,
                                  GLsizei count,
                                  GLsizei instanceCount) override;

    gl::Error drawElements(const gl::Context *context,
                           gl::PrimitiveMode mode,
                           GLsizei count,
                           GLenum type,
                           const void *indices) override;
    gl::Error drawElementsInstanced(const gl::Context *context,
                                    gl::PrimitiveMode mode,
                                    GLsizei count,
                                    GLenum type,
                                    const void *indices,
                                    GLsizei instances) override;
    gl::Error drawRangeElements(const gl::Context *context,
                                gl::PrimitiveMode mode,
                                GLuint start,
                                GLuint end,
                                GLsizei count,
                                GLenum type,
                                const void *indices) override;
    gl::Error drawArraysIndirect(const gl::Context *context,
                                 gl::PrimitiveMode mode,
                                 const void *indirect) override;
    gl::Error drawElementsIndirect(const gl::Context *context,
                                   gl::PrimitiveMode mode,
                                   GLenum type,
                                   const void *indirect) override;

    // Device loss
    GLenum getResetStatus() override;

    // Vendor and description strings.
    std::string getVendorString() const override;
    std::string getRendererDescription() const override;

    // EXT_debug_marker
    void insertEventMarker(GLsizei length, const char *marker) override;
    void pushGroupMarker(GLsizei length, const char *marker) override;
    void popGroupMarker() override;

    // KHR_debug
    void pushDebugGroup(GLenum source, GLuint id, GLsizei length, const char *message) override;
    void popDebugGroup() override;

    // State sync with dirty bits.
    gl::Error syncState(const gl::Context *context, const gl::State::DirtyBits &dirtyBits) override;

    // Disjoint timer queries
    GLint getGPUDisjoint() override;
    GLint64 getTimestamp() override;

    // Context switching
    gl::Error onMakeCurrent(const gl::Context *context) override;

    // Caps queries
    gl::Caps getNativeCaps() const override;
    const gl::TextureCapsMap &getNativeTextureCaps() const override;
    const gl::Extensions &getNativeExtensions() const override;
    const gl::Limitations &getNativeLimitations() const override;

    Renderer11 *getRenderer() const { return mRenderer; }

    gl::Error dispatchCompute(const gl::Context *context,
                              GLuint numGroupsX,
                              GLuint numGroupsY,
                              GLuint numGroupsZ) override;
    gl::Error dispatchComputeIndirect(const gl::Context *context, GLintptr indirect) override;

    gl::Error memoryBarrier(const gl::Context *context, GLbitfield barriers) override;
    gl::Error memoryBarrierByRegion(const gl::Context *context, GLbitfield barriers) override;

    angle::Result triggerDrawCallProgramRecompilation(const gl::Context *context,
                                                      gl::PrimitiveMode drawMode);

    angle::Result getIncompleteTexture(const gl::Context *context,
                                       gl::TextureType type,
                                       gl::Texture **textureOut);

    gl::Error initializeMultisampleTextureToBlack(const gl::Context *context,
                                                  gl::Texture *glTexture) override;

    void handleError(HRESULT hr,
                     const char *message,
                     const char *file,
                     const char *function,
                     unsigned int line) override;

    // TODO(jmadill): Remove this once refactor is complete. http://anglebug.com/2738
    void handleError(const gl::Error &error);

  private:
    angle::Result prepareForDrawCall(const gl::Context *context,
                                     const gl::DrawCallParams &drawCallParams);

    Renderer11 *mRenderer;
    IncompleteTextureSet mIncompleteTextures;
};
}  // namespace rx

#endif  // LIBANGLE_RENDERER_D3D_D3D11_CONTEXT11_H_
