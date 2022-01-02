/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_ImageMemoryReporter_h
#define mozilla_image_ImageMemoryReporter_h

#include <cstdint>
#include "nsString.h"
#include "mozilla/layers/SharedSurfacesMemoryReport.h"

class nsISupports;
class nsIHandleReportCallback;

namespace mozilla {
namespace image {
struct ImageMemoryCounter;
struct SurfaceMemoryCounter;

class ImageMemoryReporter final {
 public:
  /**
   * Initializes image related memory reporting in the compositor process when
   * using WebRender.
   */
  static void InitForWebRender();

  /**
   * Tears down image related memory reporting in the compositor process when
   * using WebRender.
   */
  static void ShutdownForWebRender();

  /**
   * Report all remaining entries in the shared surface's memory report. This
   * should be used by the content or main process to allow reporting any
   * entries that is was unable to cross reference with the local surface cache.
   * These are candidates for having been leaked. This should be used in
   * conjunction with AppendSharedSurfacePrefix and/or TrimSharedSurfaces to
   * produce the expected result.
   */
  static void ReportSharedSurfaces(
      nsIHandleReportCallback* aHandleReport, nsISupports* aData,
      const layers::SharedSurfacesMemoryReport& aSharedSurfaces);

  /**
   * Adjust the path prefix for a surface to include any additional metadata for
   * the shared surface, if any. It will also remove any corresponding entries
   * in the given memory report.
   */
  static void AppendSharedSurfacePrefix(
      nsACString& aPathPrefix, const SurfaceMemoryCounter& aCounter,
      layers::SharedSurfacesMemoryReport& aSharedSurfaces);

  /**
   * Remove all entries in the memory report for the given set of surfaces for
   * an image. This is useful when we aren't reporting on a particular image
   * because it isn't notable.
   */
  static void TrimSharedSurfaces(
      const ImageMemoryCounter& aCounter,
      layers::SharedSurfacesMemoryReport& aSharedSurfaces);

 private:
  /**
   * Report all remaining entries in the shared surface's memory report.
   *
   * aIsForCompositor controls how to interpret what remains in the report. If
   * true, this should mirror exactly what is currently in
   * SharedSurfacesParent's cache. This will report entries that are currently
   * mapped into the compositor process. If false, then we are in a content or
   * main process, and it should have removed entries that also exist in its
   * local surface cache -- thus any remaining entries are those that are
   * candidates for leaks.
   */
  static void ReportSharedSurfaces(
      nsIHandleReportCallback* aHandleReport, nsISupports* aData,
      bool aIsForCompositor,
      const layers::SharedSurfacesMemoryReport& aSharedSurfaces);

  static void ReportSharedSurface(
      nsIHandleReportCallback* aHandleReport, nsISupports* aData,
      bool aIsForCompositor, uint64_t aExternalId,
      const layers::SharedSurfacesMemoryReport::SurfaceEntry& aEntry);

  class WebRenderReporter;
  static WebRenderReporter* sWrReporter;
};

}  // namespace image
}  // namespace mozilla

#endif  // mozilla_image_ImageMemoryReporter_h
