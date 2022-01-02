/*
 * Copyright 2019 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/dawn/GrDawnOpsRenderPass.h"

#include "src/gpu/GrFixedClip.h"
#include "src/gpu/GrMesh.h"
#include "src/gpu/GrOpFlushState.h"
#include "src/gpu/GrPipeline.h"
#include "src/gpu/GrRenderTargetPriv.h"
#include "src/gpu/GrTexturePriv.h"
#include "src/gpu/dawn/GrDawnBuffer.h"
#include "src/gpu/dawn/GrDawnGpu.h"
#include "src/gpu/dawn/GrDawnProgramBuilder.h"
#include "src/gpu/dawn/GrDawnRenderTarget.h"
#include "src/gpu/dawn/GrDawnStencilAttachment.h"
#include "src/gpu/dawn/GrDawnTexture.h"
#include "src/gpu/dawn/GrDawnUtil.h"
#include "src/sksl/SkSLCompiler.h"

////////////////////////////////////////////////////////////////////////////////

static dawn::LoadOp to_dawn_load_op(GrLoadOp loadOp) {
    switch (loadOp) {
        case GrLoadOp::kLoad:
            return dawn::LoadOp::Load;
        case GrLoadOp::kDiscard:
            // Use LoadOp::Load to emulate DontCare.
            // Dawn doesn't have DontCare, for security reasons.
            // Load should be equivalent to DontCare for desktop; Clear would
            // probably be better for tilers. If Dawn does add DontCare
            // as an extension, use it here.
            return dawn::LoadOp::Load;
        case GrLoadOp::kClear:
            return dawn::LoadOp::Clear;
        default:
            SK_ABORT("Invalid LoadOp");
    }
}

GrDawnOpsRenderPass::GrDawnOpsRenderPass(GrDawnGpu* gpu, GrRenderTarget* rt, GrSurfaceOrigin origin,
                                         const LoadAndStoreInfo& colorInfo,
                                         const StencilLoadAndStoreInfo& stencilInfo)
        : INHERITED(rt, origin)
        , fGpu(gpu)
        , fColorInfo(colorInfo) {
    fEncoder = fGpu->device().CreateCommandEncoder();
    dawn::LoadOp colorOp = to_dawn_load_op(colorInfo.fLoadOp);
    dawn::LoadOp stencilOp = to_dawn_load_op(stencilInfo.fLoadOp);
    fPassEncoder = beginRenderPass(colorOp, stencilOp);
}

dawn::RenderPassEncoder GrDawnOpsRenderPass::beginRenderPass(dawn::LoadOp colorOp,
                                                             dawn::LoadOp stencilOp) {
    dawn::Texture texture = static_cast<GrDawnRenderTarget*>(fRenderTarget)->texture();
    auto stencilAttachment = static_cast<GrDawnStencilAttachment*>(
        fRenderTarget->renderTargetPriv().getStencilAttachment());
    dawn::TextureView colorView = texture.CreateView();
    const float *c = fColorInfo.fClearColor.vec();

    dawn::RenderPassColorAttachmentDescriptor colorAttachment;
    colorAttachment.attachment = colorView;
    colorAttachment.resolveTarget = nullptr;
    colorAttachment.clearColor = { c[0], c[1], c[2], c[3] };
    colorAttachment.loadOp = colorOp;
    colorAttachment.storeOp = dawn::StoreOp::Store;
    dawn::RenderPassColorAttachmentDescriptor* colorAttachments = { &colorAttachment };
    dawn::RenderPassDescriptor renderPassDescriptor;
    renderPassDescriptor.colorAttachmentCount = 1;
    renderPassDescriptor.colorAttachments = colorAttachments;
    if (stencilAttachment) {
        dawn::RenderPassDepthStencilAttachmentDescriptor depthStencilAttachment;
        depthStencilAttachment.attachment = stencilAttachment->view();
        depthStencilAttachment.depthLoadOp = stencilOp;
        depthStencilAttachment.stencilLoadOp = stencilOp;
        depthStencilAttachment.clearDepth = 1.0f;
        depthStencilAttachment.clearStencil = 0;
        depthStencilAttachment.depthStoreOp = dawn::StoreOp::Store;
        depthStencilAttachment.stencilStoreOp = dawn::StoreOp::Store;
        renderPassDescriptor.depthStencilAttachment = &depthStencilAttachment;
    } else {
        renderPassDescriptor.depthStencilAttachment = nullptr;
    }
    return fEncoder.BeginRenderPass(&renderPassDescriptor);
}

GrDawnOpsRenderPass::~GrDawnOpsRenderPass() {
}

GrGpu* GrDawnOpsRenderPass::gpu() { return fGpu; }

void GrDawnOpsRenderPass::end() {
    fPassEncoder.EndPass();
}

void GrDawnOpsRenderPass::submit() {
    fGpu->appendCommandBuffer(fEncoder.Finish());
}

void GrDawnOpsRenderPass::insertEventMarker(const char* msg) {
    SkASSERT(!"unimplemented");
}

void GrDawnOpsRenderPass::onClearStencilClip(const GrFixedClip& clip, bool insideStencilMask) {
    fPassEncoder.EndPass();
    fPassEncoder = beginRenderPass(dawn::LoadOp::Load, dawn::LoadOp::Clear);
}

void GrDawnOpsRenderPass::onClear(const GrFixedClip& clip, const SkPMColor4f& color) {
    fPassEncoder.EndPass();
    fPassEncoder = beginRenderPass(dawn::LoadOp::Clear, dawn::LoadOp::Load);
}

////////////////////////////////////////////////////////////////////////////////

void GrDawnOpsRenderPass::inlineUpload(GrOpFlushState* state,
                                       GrDeferredTextureUploadFn& upload) {
    SkASSERT(!"unimplemented");
}

////////////////////////////////////////////////////////////////////////////////

void GrDawnOpsRenderPass::setScissorState(const GrProgramInfo& programInfo) {
    SkIRect rect;
    if (programInfo.pipeline().isScissorEnabled()) {
        constexpr SkIRect kBogusScissor{0, 0, 1, 1};
        rect = programInfo.hasFixedScissor() ? programInfo.fixedScissor() : kBogusScissor;
        if (kBottomLeft_GrSurfaceOrigin == fOrigin) {
            rect.setXYWH(rect.x(), fRenderTarget->height() - rect.bottom(),
                         rect.width(), rect.height());
        }
    } else {
        rect = SkIRect::MakeWH(fRenderTarget->width(), fRenderTarget->height());
    }
    fPassEncoder.SetScissorRect(rect.x(), rect.y(), rect.width(), rect.height());
}

void GrDawnOpsRenderPass::applyState(const GrProgramInfo& programInfo,
                                     const GrPrimitiveType primitiveType) {
    sk_sp<GrDawnProgram> program = fGpu->getOrCreateRenderPipeline(fRenderTarget,
                                                                   programInfo,
                                                                   primitiveType);
    auto bindGroup = program->setData(fGpu, fRenderTarget, programInfo);
    fPassEncoder.SetPipeline(program->fRenderPipeline);
    fPassEncoder.SetBindGroup(0, bindGroup, 0, nullptr);
    const GrPipeline& pipeline = programInfo.pipeline();
    if (pipeline.isStencilEnabled()) {
        fPassEncoder.SetStencilReference(pipeline.getUserStencil()->fFront.fRef);
    }
    GrXferProcessor::BlendInfo blendInfo = pipeline.getXferProcessor().getBlendInfo();
    const float* c = blendInfo.fBlendConstant.vec();
    dawn::Color color{c[0], c[1], c[2], c[3]};
    fPassEncoder.SetBlendColor(&color);
    this->setScissorState(programInfo);
}

void GrDawnOpsRenderPass::onDraw(const GrProgramInfo& programInfo,
                                 const GrMesh meshes[],
                                 int meshCount,
                                 const SkRect& bounds) {
    if (!meshCount) {
        return;
    }
    for (int i = 0; i < meshCount; ++i) {
        applyState(programInfo, meshes[0].primitiveType());
        meshes[i].sendToGpu(this);
    }
}

void GrDawnOpsRenderPass::sendInstancedMeshToGpu(GrPrimitiveType,
                                                 const GrBuffer* vertexBuffer,
                                                 int vertexCount,
                                                 int baseVertex,
                                                 const GrBuffer* instanceBuffer,
                                                 int instanceCount,
                                                 int baseInstance) {
    dawn::Buffer vb = static_cast<const GrDawnBuffer*>(vertexBuffer)->get();
    fPassEncoder.SetVertexBuffer(0, vb);
    fPassEncoder.Draw(vertexCount, 1, baseVertex, baseInstance);
    fGpu->stats()->incNumDraws();
}

void GrDawnOpsRenderPass::sendIndexedInstancedMeshToGpu(GrPrimitiveType,
                                                        const GrBuffer* indexBuffer,
                                                        int indexCount,
                                                        int baseIndex,
                                                        const GrBuffer* vertexBuffer,
                                                        int baseVertex,
                                                        const GrBuffer* instanceBuffer,
                                                        int instanceCount,
                                                        int baseInstance,
                                                        GrPrimitiveRestart restart) {
    dawn::Buffer vb = static_cast<const GrDawnBuffer*>(vertexBuffer)->get();
    dawn::Buffer ib = static_cast<const GrDawnBuffer*>(indexBuffer)->get();
    fPassEncoder.SetIndexBuffer(ib);
    fPassEncoder.SetVertexBuffer(0, vb);
    fPassEncoder.DrawIndexed(indexCount, 1, baseIndex, baseVertex, baseInstance);
    fGpu->stats()->incNumDraws();
}
