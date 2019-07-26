/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageMemoryReporter.h"
#include "Image.h"
#include "mozilla/layers/SharedSurfacesParent.h"
#include "mozilla/StaticPrefs_image.h"
#include "nsIMemoryReporter.h"
#include "nsISupportsImpl.h"

namespace mozilla {
namespace image {

ImageMemoryReporter::WebRenderReporter* ImageMemoryReporter::sWrReporter;

class ImageMemoryReporter::WebRenderReporter final : public nsIMemoryReporter {
 public:
  NS_DECL_ISUPPORTS

  WebRenderReporter() {}

  NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                            nsISupports* aData, bool aAnonymize) override {
    layers::SharedSurfacesMemoryReport report;
    layers::SharedSurfacesParent::AccumulateMemoryReport(report);
    ReportSharedSurfaces(aHandleReport, aData, /* aIsForCompositor */ true,
                         report);
    return NS_OK;
  }

 private:
  virtual ~WebRenderReporter() {}
};

NS_IMPL_ISUPPORTS(ImageMemoryReporter::WebRenderReporter, nsIMemoryReporter)

/* static */
void ImageMemoryReporter::InitForWebRender() {
  MOZ_ASSERT(XRE_IsParentProcess() || XRE_IsGPUProcess());
  if (!sWrReporter) {
    sWrReporter = new WebRenderReporter();
    RegisterStrongMemoryReporter(sWrReporter);
  }
}

/* static */
void ImageMemoryReporter::ShutdownForWebRender() {
  MOZ_ASSERT(XRE_IsParentProcess() || XRE_IsGPUProcess());
  if (sWrReporter) {
    UnregisterStrongMemoryReporter(sWrReporter);
    sWrReporter = nullptr;
  }
}

/* static */
void ImageMemoryReporter::ReportSharedSurfaces(
    nsIHandleReportCallback* aHandleReport, nsISupports* aData,
    const layers::SharedSurfacesMemoryReport& aSharedSurfaces) {
  ReportSharedSurfaces(aHandleReport, aData,
                       /* aIsForCompositor */ false, aSharedSurfaces);
}

/* static */
void ImageMemoryReporter::ReportSharedSurfaces(
    nsIHandleReportCallback* aHandleReport, nsISupports* aData,
    bool aIsForCompositor,
    const layers::SharedSurfacesMemoryReport& aSharedSurfaces) {
  MOZ_ASSERT_IF(aIsForCompositor, XRE_IsParentProcess() || XRE_IsGPUProcess());
  MOZ_ASSERT_IF(!aIsForCompositor,
                XRE_IsParentProcess() || XRE_IsContentProcess());

  for (auto i = aSharedSurfaces.mSurfaces.begin();
       i != aSharedSurfaces.mSurfaces.end(); ++i) {
    ReportSharedSurface(aHandleReport, aData, aIsForCompositor, i->first,
                        i->second);
  }
}

/* static */
void ImageMemoryReporter::ReportSharedSurface(
    nsIHandleReportCallback* aHandleReport, nsISupports* aData,
    bool aIsForCompositor, uint64_t aExternalId,
    const layers::SharedSurfacesMemoryReport::SurfaceEntry& aEntry) {
  nsAutoCString path;
  if (aIsForCompositor) {
    path.AppendLiteral("gfx/webrender/images/mapped_from_owner/");
  } else {
    path.AppendLiteral("gfx/webrender/images/owner_cache_missing/");
  }

  if (aIsForCompositor) {
    path.AppendLiteral("pid=");
    path.AppendInt(uint32_t(aEntry.mCreatorPid));
    path.AppendLiteral("/");
  }

  if (StaticPrefs::image_mem_debug_reporting()) {
    path.AppendInt(aExternalId, 16);
    path.AppendLiteral("/");
  }

  path.AppendLiteral("image(");
  path.AppendInt(aEntry.mSize.width);
  path.AppendLiteral("x");
  path.AppendInt(aEntry.mSize.height);
  path.AppendLiteral(", compositor_ref:");
  path.AppendInt(aEntry.mConsumers);
  path.AppendLiteral(", creator_ref:");
  path.AppendInt(aEntry.mCreatorRef);
  path.AppendLiteral(")/decoded-nonheap");

  size_t surfaceSize = mozilla::ipc::SharedMemory::PageAlignedSize(
      aEntry.mSize.height * aEntry.mStride);

  // If this memory has already been reported elsewhere (e.g. as part of our
  // explicit section in the surface cache), we don't want report it again as
  // KIND_NONHEAP and have it counted again.
  bool sameProcess = aEntry.mCreatorPid == base::GetCurrentProcId();
  int32_t kind = aIsForCompositor && !sameProcess
                     ? nsIMemoryReporter::KIND_NONHEAP
                     : nsIMemoryReporter::KIND_OTHER;

  NS_NAMED_LITERAL_CSTRING(desc, "Decoded image data stored in shared memory.");
  aHandleReport->Callback(EmptyCString(), path, kind,
                          nsIMemoryReporter::UNITS_BYTES, surfaceSize, desc,
                          aData);
}

/* static */
void ImageMemoryReporter::AppendSharedSurfacePrefix(
    nsACString& aPathPrefix, const SurfaceMemoryCounter& aCounter,
    layers::SharedSurfacesMemoryReport& aSharedSurfaces) {
  uint64_t extId = aCounter.Values().ExternalId();
  if (extId) {
    auto gpuEntry = aSharedSurfaces.mSurfaces.find(extId);

    if (StaticPrefs::image_mem_debug_reporting()) {
      aPathPrefix.AppendLiteral(", external_id:");
      aPathPrefix.AppendInt(extId, 16);
      if (gpuEntry != aSharedSurfaces.mSurfaces.end()) {
        aPathPrefix.AppendLiteral(", compositor_ref:");
        aPathPrefix.AppendInt(gpuEntry->second.mConsumers);
      } else {
        aPathPrefix.AppendLiteral(", compositor_ref:missing");
      }
    }

    if (gpuEntry != aSharedSurfaces.mSurfaces.end()) {
      MOZ_ASSERT(gpuEntry->second.mCreatorRef);
      aSharedSurfaces.mSurfaces.erase(gpuEntry);
    }
  }
}

/* static */
void ImageMemoryReporter::TrimSharedSurfaces(
    const ImageMemoryCounter& aCounter,
    layers::SharedSurfacesMemoryReport& aSharedSurfaces) {
  if (aSharedSurfaces.mSurfaces.empty()) {
    return;
  }

  for (const SurfaceMemoryCounter& counter : aCounter.Surfaces()) {
    uint64_t extId = counter.Values().ExternalId();
    if (extId) {
      auto gpuEntry = aSharedSurfaces.mSurfaces.find(extId);
      if (gpuEntry != aSharedSurfaces.mSurfaces.end()) {
        MOZ_ASSERT(gpuEntry->second.mCreatorRef);
        aSharedSurfaces.mSurfaces.erase(gpuEntry);
      }
    }
  }
}

}  // namespace image
}  // namespace mozilla
