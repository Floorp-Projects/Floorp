/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_UTILS_H
#define GFX_UTILS_H

#include "gfxTypes.h"
#include "ImageTypes.h"
#include "imgIContainer.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "nsColor.h"
#include "nsPrintfCString.h"
#include "nsRegionFwd.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/webrender/WebRenderTypes.h"

class gfxASurface;
class gfxDrawable;
struct gfxQuad;
class nsIInputStream;
class nsIGfxInfo;

namespace mozilla {
namespace dom {
class Element;
}
namespace layers {
class WebRenderBridgeChild;
class GlyphArray;
struct PlanarYCbCrData;
class WebRenderCommand;
}  // namespace layers
namespace image {
class ImageRegion;
}  // namespace image
namespace wr {
class DisplayListBuilder;
}  // namespace wr
}  // namespace mozilla

enum class ImageType {
  BMP,
  ICO,
  JPEG,
  PNG,
};

class gfxUtils {
 public:
  typedef mozilla::gfx::DataSourceSurface DataSourceSurface;
  typedef mozilla::gfx::DrawTarget DrawTarget;
  typedef mozilla::gfx::IntPoint IntPoint;
  typedef mozilla::gfx::Matrix Matrix;
  typedef mozilla::gfx::SourceSurface SourceSurface;
  typedef mozilla::gfx::SurfaceFormat SurfaceFormat;
  typedef mozilla::image::ImageRegion ImageRegion;

  /*
   * Premultiply or Unpremultiply aSourceSurface, writing the result
   * to aDestSurface or back into aSourceSurface if aDestSurface is null.
   *
   * If aDestSurface is given, it must have identical format, dimensions, and
   * stride as the source.
   *
   * If the source is not SurfaceFormat::A8R8G8B8_UINT32, no operation is
   * performed.  If aDestSurface is given, the data is copied over.
   */
  static bool PremultiplyDataSurface(DataSourceSurface* srcSurf,
                                     DataSourceSurface* destSurf);
  static bool UnpremultiplyDataSurface(DataSourceSurface* srcSurf,
                                       DataSourceSurface* destSurf);

  static already_AddRefed<DataSourceSurface> CreatePremultipliedDataSurface(
      DataSourceSurface* srcSurf);
  static already_AddRefed<DataSourceSurface> CreateUnpremultipliedDataSurface(
      DataSourceSurface* srcSurf);

  static void ConvertBGRAtoRGBA(uint8_t* aData, uint32_t aLength);

  /**
   * Draw something drawable while working around limitations like bad support
   * for EXTEND_PAD, lack of source-clipping, or cairo / pixman bugs with
   * extreme user-space-to-image-space transforms.
   *
   * The input parameters here usually come from the output of our image
   * snapping algorithm in nsLayoutUtils.cpp.
   * This method is split from nsLayoutUtils::DrawPixelSnapped to allow for
   * adjusting the parameters. For example, certain images with transparent
   * margins only have a drawable subimage. For those images, imgFrame::Draw
   * will tweak the rects and transforms that it gets from the pixel snapping
   * algorithm before passing them on to this method.
   */
  static void DrawPixelSnapped(gfxContext* aContext, gfxDrawable* aDrawable,
                               const gfxSize& aImageSize,
                               const ImageRegion& aRegion,
                               const mozilla::gfx::SurfaceFormat aFormat,
                               mozilla::gfx::SamplingFilter aSamplingFilter,
                               uint32_t aImageFlags = imgIContainer::FLAG_NONE,
                               gfxFloat aOpacity = 1.0,
                               bool aUseOptimalFillOp = true);

  /**
   * Clip aContext to the region aRegion.
   */
  static void ClipToRegion(gfxContext* aContext, const nsIntRegion& aRegion);

  /**
   * Clip aTarget to the region aRegion.
   */
  static void ClipToRegion(mozilla::gfx::DrawTarget* aTarget,
                           const nsIntRegion& aRegion);

  /*
   * Convert image format to depth value
   */
  static int ImageFormatToDepth(gfxImageFormat aFormat);

  /**
   * Return the transform matrix that maps aFrom to the rectangle defined by
   * aToTopLeft/aToTopRight/aToBottomRight. aFrom must be
   * nonempty and the destination rectangle must be axis-aligned.
   */
  static gfxMatrix TransformRectToRect(const gfxRect& aFrom,
                                       const gfxPoint& aToTopLeft,
                                       const gfxPoint& aToTopRight,
                                       const gfxPoint& aToBottomRight);

  static Matrix TransformRectToRect(const gfxRect& aFrom,
                                    const IntPoint& aToTopLeft,
                                    const IntPoint& aToTopRight,
                                    const IntPoint& aToBottomRight);

  /**
   * If aIn can be represented exactly using an gfx::IntRect (i.e.
   * integer-aligned edges and coordinates in the int32_t range) then we
   * set aOut to that rectangle, otherwise return failure.
   */
  static bool GfxRectToIntRect(const gfxRect& aIn, mozilla::gfx::IntRect* aOut);

  /* Conditions this border to Cairo's max coordinate space.
   * The caller can check IsEmpty() after Condition() -- if it's TRUE,
   * the caller can possibly avoid doing any extra rendering.
   */
  static void ConditionRect(gfxRect& aRect);

  /*
   * Transform this rectangle with aMatrix, resulting in a gfxQuad.
   */
  static gfxQuad TransformToQuad(const gfxRect& aRect,
                                 const mozilla::gfx::Matrix4x4& aMatrix);

  /**
   * Return the smallest power of kScaleResolution (2) greater than or equal to
   * aVal. If aRoundDown is specified, the power of 2 will rather be less than
   * or equal to aVal.
   */
  static float ClampToScaleFactor(float aVal, bool aRoundDown = false);

  /**
   * Clears surface to aColor (which defaults to transparent black).
   */
  static void ClearThebesSurface(gfxASurface* aSurface);

  static const float* YuvToRgbMatrix4x3RowMajor(
      mozilla::gfx::YUVColorSpace aYUVColorSpace);
  static const float* YuvToRgbMatrix3x3ColumnMajor(
      mozilla::gfx::YUVColorSpace aYUVColorSpace);
  static const float* YuvToRgbMatrix4x4ColumnMajor(
      mozilla::gfx::YUVColorSpace aYUVColorSpace);

  /**
   * Creates a copy of aSurface, but having the SurfaceFormat aFormat.
   *
   * This function always creates a new surface. Do not call it if aSurface's
   * format is the same as aFormat. Such a non-conversion would just be an
   * unnecessary and wasteful copy (this function asserts to prevent that).
   *
   * This function is intended to be called by code that needs to access the
   * pixel data of the surface, but doesn't want to have lots of branches
   * to handle different pixel data formats (code which would become out of
   * date if and when new formats are added). Callers can use this function
   * to copy the surface to a specified format so that they only have to
   * handle pixel data in that one format.
   *
   * WARNING: There are format conversions that will not be supported by this
   * function. It very much depends on what the Moz2D backends support. If
   * the temporary B8G8R8A8 DrawTarget that this function creates has a
   * backend that supports DrawSurface() calls passing a surface with
   * aSurface's format it will work. Otherwise it will not.
   *
   *                      *** IMPORTANT PERF NOTE ***
   *
   * This function exists partly because format conversion is fraught with
   * non-obvious performance hazards, so we don't want Moz2D consumers to be
   * doing their own format conversion. Do not try to do so, or at least read
   * the comments in this functions implemtation. That said, the copy that
   * this function carries out has a cost and, although this function tries
   * to avoid perf hazards such as expensive uploads to/readbacks from the
   * GPU, it can't guarantee that it always successfully does so. Perf
   * critical code that can directly handle the common formats that it
   * encounters in a way that is cheaper than a copy-with-format-conversion
   * should consider doing so, and only use this function as a fallback to
   * handle other formats.
   *
   * XXXjwatt it would be nice if SourceSurface::GetDataSurface took a
   * SurfaceFormat argument (with a default argument meaning "use the
   * existing surface's format") and returned a DataSourceSurface in that
   * format. (There would then be an issue of callers maybe failing to
   * realize format conversion may involve expensive copying/uploading/
   * readback.)
   */
  static already_AddRefed<DataSourceSurface>
  CopySurfaceToDataSourceSurfaceWithFormat(SourceSurface* aSurface,
                                           SurfaceFormat aFormat);

  /**
   * Return a color that can be used to identify a frame with a given frame
   * number. The colors will cycle after sNumFrameColors.  You can query colors
   * 0 .. sNumFrameColors-1 to get all the colors back.
   */
  static const mozilla::gfx::Color& GetColorForFrameNumber(
      uint64_t aFrameNumber);
  static const uint32_t sNumFrameColors;

  enum BinaryOrData { eBinaryEncode, eDataURIEncode };

  /**
   * Encodes the given surface to PNG/JPEG/BMP/etc. using imgIEncoder.
   * If both aFile and aString are null, the encoded data is copied to the
   * clipboard.
   *
   * @param aImageType The image type that the surface is to be encoded to.
   *   Used to create an appropriate imgIEncoder instance to do the encoding.
   *
   * @param aOutputOptions Passed directly to imgIEncoder::InitFromData as
   *   the value of the |outputOptions| parameter. Callers are responsible
   *   for making sure that this is a sane value for the passed MIME-type
   *   (i.e. for the type of encoder that will be created).
   *
   * @aBinaryOrData Flag used to determine if the surface is simply encoded
   *   to the requested binary image format, or if the binary image is
   *   further converted to base-64 and written out as a 'data:' URI.
   *
   * @aFile If specified, the encoded data is written out to aFile.
   *
   * @aString If specified, the encoded data is written out to aString.
   *
   * TODO: Copying to the clipboard as a binary file is not currently
   * supported.
   */
  static nsresult EncodeSourceSurface(SourceSurface* aSurface,
                                      const ImageType aImageType,
                                      const nsAString& aOutputOptions,
                                      BinaryOrData aBinaryOrData, FILE* aFile,
                                      nsACString* aString = nullptr);

  /**
   * Write as a PNG file to the path aFile.
   */
  static void WriteAsPNG(SourceSurface* aSurface, const nsAString& aFile);
  static void WriteAsPNG(SourceSurface* aSurface, const char* aFile);
  static void WriteAsPNG(DrawTarget* aDT, const nsAString& aFile);
  static void WriteAsPNG(DrawTarget* aDT, const char* aFile);

  /**
   * Dump as a PNG encoded Data URL to a FILE stream (using stdout by
   * default).
   *
   * Rather than giving aFile a default argument we have separate functions
   * to make them easier to use from a debugger.
   */
  static void DumpAsDataURI(SourceSurface* aSourceSurface, FILE* aFile);
  static inline void DumpAsDataURI(SourceSurface* aSourceSurface) {
    DumpAsDataURI(aSourceSurface, stdout);
  }
  static void DumpAsDataURI(DrawTarget* aDT, FILE* aFile);
  static inline void DumpAsDataURI(DrawTarget* aDT) {
    DumpAsDataURI(aDT, stdout);
  }
  static nsCString GetAsDataURI(SourceSurface* aSourceSurface);
  static nsCString GetAsDataURI(DrawTarget* aDT);
  static nsCString GetAsLZ4Base64Str(DataSourceSurface* aSourceSurface);

  static mozilla::UniquePtr<uint8_t[]> GetImageBuffer(
      DataSourceSurface* aSurface, bool aIsAlphaPremultiplied,
      int32_t* outFormat);

  static nsresult GetInputStream(DataSourceSurface* aSurface,
                                 bool aIsAlphaPremultiplied,
                                 const char* aMimeType,
                                 const nsAString& aEncoderOptions,
                                 nsIInputStream** outStream);

  static nsresult ThreadSafeGetFeatureStatus(
      const nsCOMPtr<nsIGfxInfo>& gfxInfo, int32_t feature,
      nsACString& failureId, int32_t* status);

  static void RemoveShaderCacheFromDiskIfNecessary();

  /**
   * Copy to the clipboard as a PNG encoded Data URL.
   */
  static void CopyAsDataURI(SourceSurface* aSourceSurface);
  static void CopyAsDataURI(DrawTarget* aDT);

  static bool DumpDisplayList();

  static FILE* sDumpPaintFile;

  static mozilla::wr::RenderRoot GetContentRenderRoot();

  static mozilla::Maybe<mozilla::wr::RenderRoot> GetRenderRootForFrame(
      const nsIFrame* aFrame);
  static mozilla::Maybe<mozilla::wr::RenderRoot> GetRenderRootForElement(
      const mozilla::dom::Element* aElement);
  static mozilla::wr::RenderRoot RecursivelyGetRenderRootForFrame(
      const nsIFrame* aFrame);
  static mozilla::wr::RenderRoot RecursivelyGetRenderRootForElement(
      const mozilla::dom::Element* aElement);
};

namespace mozilla {

struct StyleRGBA;

namespace gfx {

/**
 * If the CMS mode is eCMSMode_All, these functions transform the passed
 * color to a device color using the transform returened by gfxPlatform::
 * GetCMSRGBTransform().  If the CMS mode is some other value, the color is
 * returned unchanged (other than a type change to Moz2D Color, if
 * applicable).
 */
Color ToDeviceColor(Color aColor);
Color ToDeviceColor(const StyleRGBA& aColor);
Color ToDeviceColor(nscolor aColor);

/**
 * Performs a checked multiply of the given width, height, and bytes-per-pixel
 * values.
 */
static inline CheckedInt<uint32_t> SafeBytesForBitmap(uint32_t aWidth,
                                                      uint32_t aHeight,
                                                      unsigned aBytesPerPixel) {
  MOZ_ASSERT(aBytesPerPixel > 0);
  CheckedInt<uint32_t> width = uint32_t(aWidth);
  CheckedInt<uint32_t> height = uint32_t(aHeight);
  return width * height * aBytesPerPixel;
}

}  // namespace gfx
}  // namespace mozilla

#endif
