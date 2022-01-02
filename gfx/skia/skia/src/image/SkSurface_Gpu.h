/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkSurface_Gpu_DEFINED
#define SkSurface_Gpu_DEFINED

#include "include/private/GrTypesPriv.h"
#include "src/image/SkSurface_Base.h"

#if SK_SUPPORT_GPU

class GrBackendFormat;
class SkGpuDevice;

class SkSurface_Gpu : public SkSurface_Base {
public:
    SkSurface_Gpu(sk_sp<SkGpuDevice>);
    ~SkSurface_Gpu() override;

    // This is an internal-only factory
    static sk_sp<SkSurface> MakeWrappedRenderTarget(GrContext*,
                                                    std::unique_ptr<GrRenderTargetContext>);

    GrBackendTexture onGetBackendTexture(BackendHandleAccess) override;
    GrBackendRenderTarget onGetBackendRenderTarget(BackendHandleAccess) override;
    bool onReplaceBackendTexture(const GrBackendTexture&, GrSurfaceOrigin, TextureReleaseProc,
                                 ReleaseContext) override;

    SkCanvas* onNewCanvas() override;
    sk_sp<SkSurface> onNewSurface(const SkImageInfo&) override;
    sk_sp<SkImage> onNewImageSnapshot(const SkIRect* subset) override;
    void onWritePixels(const SkPixmap&, int x, int y) override;
    void onAsyncRescaleAndReadPixels(const SkImageInfo& info, const SkIRect& srcRect,
                                     RescaleGamma rescaleGamma, SkFilterQuality rescaleQuality,
                                     ReadPixelsCallback callback,
                                     ReadPixelsContext context) override;
    void onAsyncRescaleAndReadPixelsYUV420(SkYUVColorSpace yuvColorSpace,
                                           sk_sp<SkColorSpace> dstColorSpace,
                                           const SkIRect& srcRect,
                                           const SkISize& dstSize,
                                           RescaleGamma rescaleGamma,
                                           SkFilterQuality rescaleQuality,
                                           ReadPixelsCallback callback,
                                           ReadPixelsContext context) override;

    void onCopyOnWrite(ContentChangeMode) override;
    void onDiscard() override;
    GrSemaphoresSubmitted onFlush(BackendSurfaceAccess access, const GrFlushInfo& info) override;
    bool onWait(int numSemaphores, const GrBackendSemaphore* waitSemaphores) override;
    bool onCharacterize(SkSurfaceCharacterization*) const override;
    bool onIsCompatible(const SkSurfaceCharacterization&) const override;
    void onDraw(SkCanvas* canvas, SkScalar x, SkScalar y, const SkPaint* paint) override;
    bool onDraw(const SkDeferredDisplayList*) override;

    SkGpuDevice* getDevice() { return fDevice.get(); }

private:
    sk_sp<SkGpuDevice> fDevice;

    typedef SkSurface_Base INHERITED;
};

#endif // SK_SUPPORT_GPU

#endif // SkSurface_Gpu_DEFINED
