/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageBitmapUtils.h"
#include "ImageBitmapColorUtils.h"
#include "ImageContainer.h"
#include "libyuv.h"
#include "mozilla/dom/ImageBitmapBinding.h"
#include "mozilla/gfx/2D.h"

#include <functional>

using namespace libyuv;
using namespace mozilla::gfx;

namespace mozilla {
namespace dom {
namespace imagebitmapformat {

class Utils;
class Utils_RGBA32;
class Utils_BGRA32;
class Utils_RGB24;
class Utils_BGR24;
class Utils_Gray8;
class Utils_YUV444P;
class Utils_YUV422P;
class Utils_YUV420P;
class Utils_YUV420SP_NV12;
class Utils_YUV420SP_NV21;
class Utils_HSV;
class Utils_Lab;
class Utils_Depth;

static int GetBytesPerPixelValue(ChannelPixelLayoutDataType aDataType)
{
  switch (aDataType)
  {
  case ChannelPixelLayoutDataType::Uint8:
    return sizeof(uint8_t);
  case ChannelPixelLayoutDataType::Int8:
    return sizeof(int8_t);
  case ChannelPixelLayoutDataType::Uint16:
    return sizeof(uint16_t);
  case ChannelPixelLayoutDataType::Int16:
    return sizeof(int16_t);
  case ChannelPixelLayoutDataType::Uint32:
    return sizeof(uint32_t);
  case ChannelPixelLayoutDataType::Int32:
    return sizeof(int32_t);
  case ChannelPixelLayoutDataType::Float32:
    return sizeof(float);
  case ChannelPixelLayoutDataType::Float64:
    return sizeof(double);
  default:
    return 0;
  }
}

/*
 * The UtilsUniquePtr is a UniquePtr to ImageBitmapFormatUtils with a customized
 * deleter which does nothing. This is used as the return type of
 * ImageBitmapFormatUtils::GetUtils to prevent users deleting the returned
 * pointer.
 */
struct DoNotDelete { void operator()(void* p) {} };
using UtilsUniquePtr = UniquePtr<Utils, DoNotDelete>;

/*
 * ImageBitmapFormatUtils is an abstract class which provides interfaces to
 * extract information of each ImageBitmapFormat and interfaces to convert
 * image data between different ImageBitmapFormats. For each kind of
 * ImageBitmapFromat, we derive a subclass from the ImageBitmapFormatUtils to
 * implement functionalities that are subject to the specific ImageBitmapFormat.
 *
 * ImageBitmapFormatUtils is an abstract class and its sub-classes are designed
 * as singletons. The singleton instance of sub-classes could be initialized and
 * accessed via the ImageBitmapFormatUtils::GetUtils() static method. The
 * singleton instance is a static local variable which does not need to be
 * released manually and, with the C++11 static initialization, the
 * initialization is thread-safe.
 *
 * ImageBitmapFormatUtils and its sub-classes are designed to unify operations
 * of ImageBitmap-extensions over different kinds of ImageBitmapFormats; they
 * provide following functionalities:
 *
 * (1) Create default/customized ImagePixelLayout object of each kind of
 *     ImageBitmapFormat.
 * (2) Store the channel counts of each ImageBitmapFormat.
 * (3) Calculate the needed buffer size of each kind of ImageBitmapFormat with
 *     given width, height and stride.
 * (4) Perform color conversion between supported ImageBitmapFormats. We use
 *     _double dispatching_ to identify the source format and destination format
 *     at run time. The _double dispatching_ here is mainly implemented by
 *     overriding the _convertTo_ method over the ImageBitmapFormatUtils class
 *     hierarchy and overloading the _convertFrom_ methods over all sub-classes
 *     of ImageBitmapFormatUtils.
 */
class Utils
{
public:
  // Get the singleton utility instance of the given ImageBitmapFormat.
  static UtilsUniquePtr GetUtils(ImageBitmapFormat aFormat);

  // Get the needed buffer size to store image data in the current
  // ImageBitmapFormat with the given width and height.
  // The current ImageBitmapFormat is the format used to implement the concrete
  // subclass of which the current instance is initialized.
  virtual uint32_t NeededBufferSize(uint32_t width, uint32_t height) = 0;

  // Creates a default ImagePixelLayout object of the current ImageBitmapFormat
  // with the given width, height and stride.
  virtual UniquePtr<ImagePixelLayout>
  CreateDefaultLayout(uint32_t aWidth, uint32_t aHeight, uint32_t aStride) = 0;

  // Convert the source image data (stored in the aSrcBuffer and described by
  // the aSrcLayout) from the current ImageBitmapFormat to the given
  // ImageBitmapFormat, aDstFormat.
  // The converted image data is stored in the aDstBuffer and described by the
  // returned ImagePixelLayout object.
  virtual UniquePtr<ImagePixelLayout>
  ConvertTo(Utils* aDstFormat, const uint8_t* aSrcBuffer, const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer) = 0;

  // ConvertFrom():
  // Convert the source image data (which is in the aSrcFormat format, the pixel
  // layout is described by the aSrcLayout and the raw data is stored in the
  // aSrcBuffer) to the current ImageBitmapFormat.
  // The converted image data is stored in the aDstBuffer and described by the
  // returned ImagePixelLayout object.
  virtual UniquePtr<ImagePixelLayout>
  ConvertFrom(Utils_RGBA32* aSrcFormat, const uint8_t* aSrcBuffer, const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer) = 0;

  virtual UniquePtr<ImagePixelLayout>
  ConvertFrom(Utils_BGRA32* aSrcFormat, const uint8_t* aSrcBuffer, const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer) = 0;

  virtual UniquePtr<ImagePixelLayout>
  ConvertFrom(Utils_RGB24* aSrcFormat, const uint8_t* aSrcBuffer, const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer) = 0;

  virtual UniquePtr<ImagePixelLayout>
  ConvertFrom(Utils_BGR24* aSrcFormat, const uint8_t* aSrcBuffer, const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer) = 0;

  virtual UniquePtr<ImagePixelLayout>
  ConvertFrom(Utils_Gray8* aSrcFormat, const uint8_t* aSrcBuffer, const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer) = 0;

  virtual UniquePtr<ImagePixelLayout>
  ConvertFrom(Utils_YUV444P* aSrcFormat, const uint8_t* aSrcBuffer, const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer) = 0;

  virtual UniquePtr<ImagePixelLayout>
  ConvertFrom(Utils_YUV422P* aSrcFormat, const uint8_t* aSrcBuffer, const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer) = 0;

  virtual UniquePtr<ImagePixelLayout>
  ConvertFrom(Utils_YUV420P* aSrcFormat, const uint8_t* aSrcBuffer, const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer) = 0;

  virtual UniquePtr<ImagePixelLayout>
  ConvertFrom(Utils_YUV420SP_NV12* aSrcFormat, const uint8_t* aSrcBuffer, const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer) = 0;

  virtual UniquePtr<ImagePixelLayout>
  ConvertFrom(Utils_YUV420SP_NV21* aSrcFormat, const uint8_t* aSrcBuffer, const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer) = 0;

  virtual UniquePtr<ImagePixelLayout>
  ConvertFrom(Utils_HSV* aSrcFormat, const uint8_t* aSrcBuffer, const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer) = 0;

  virtual UniquePtr<ImagePixelLayout>
  ConvertFrom(Utils_Lab* aSrcFormat, const uint8_t* aSrcBuffer, const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer) = 0;

  virtual UniquePtr<ImagePixelLayout>
  ConvertFrom(Utils_Depth* aSrcFormat, const uint8_t* aSrcBuffer, const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer) = 0;

  // Check whether or not the current ImageBitmapFormat can be converted from
  // the given ImageBitmapFormat.
  virtual bool
  CanConvertFrom(ImageBitmapFormat aSrcFormat) = 0;

  // Get the number of channels.
  uint8_t GetChannelCount() const
  {
    return mChannels;
  }

protected:
  Utils(uint32_t aChannels,
                         ChannelPixelLayoutDataType aDataType)
  : mChannels(aChannels)
  , mBytesPerPixelValue(GetBytesPerPixelValue(aDataType))
  , mDataType(aDataType)
  {
  }

  virtual ~Utils()
  {
  }

  const uint8_t mChannels;
  const int mBytesPerPixelValue;
  const ChannelPixelLayoutDataType mDataType;
};

#define DECLARE_Utils(NAME)                          \
class Utils_ ## NAME : public Utils \
{                                                                     \
private:                                                              \
  explicit Utils_ ## NAME ();                        \
  ~Utils_ ## NAME () = default;                      \
  Utils_ ## NAME (Utils_ ## NAME const &) = delete;             \
  Utils_ ## NAME (Utils_ ## NAME &&) = delete;                  \
  Utils_ ## NAME & operator=(Utils_ ## NAME const &) = delete;  \
  Utils_ ## NAME & operator=(Utils_ ## NAME &&) = delete;       \
                                                                                                  \
public:                                                     \
  static Utils_ ## NAME & GetInstance();   \
                                                            \
  virtual uint32_t NeededBufferSize(uint32_t aWidth, uint32_t aHeight) override;  \
                                                                                  \
  virtual UniquePtr<ImagePixelLayout>                          \
  CreateDefaultLayout(uint32_t, uint32_t, uint32_t) override;  \
                                                               \
  virtual UniquePtr<ImagePixelLayout>                                                             \
  ConvertTo(Utils*, const uint8_t*, const ImagePixelLayout*, uint8_t*) override; \
                                                                                                  \
  virtual UniquePtr<ImagePixelLayout>                                                                       \
  ConvertFrom(Utils_RGBA32*, const uint8_t*, const ImagePixelLayout*, uint8_t*) override;  \
                                                                                                            \
  virtual UniquePtr<ImagePixelLayout>                                                                       \
  ConvertFrom(Utils_BGRA32*, const uint8_t*, const ImagePixelLayout*, uint8_t*) override;  \
                                                                                                            \
  virtual UniquePtr<ImagePixelLayout>                                                                     \
  ConvertFrom(Utils_RGB24*, const uint8_t*, const ImagePixelLayout*, uint8_t*) override; \
                                                                                                          \
  virtual UniquePtr<ImagePixelLayout>                                                                     \
  ConvertFrom(Utils_BGR24*, const uint8_t*, const ImagePixelLayout*, uint8_t*) override; \
                                                                                                          \
  virtual UniquePtr<ImagePixelLayout>                                                                     \
  ConvertFrom(Utils_Gray8*, const uint8_t*, const ImagePixelLayout*, uint8_t*) override; \
                                                                                                          \
  virtual UniquePtr<ImagePixelLayout>                                                                       \
  ConvertFrom(Utils_YUV444P*, const uint8_t*, const ImagePixelLayout*, uint8_t*) override; \
                                                                                                            \
  virtual UniquePtr<ImagePixelLayout>                                                                       \
  ConvertFrom(Utils_YUV422P*, const uint8_t*, const ImagePixelLayout*, uint8_t*) override; \
                                                                                                            \
  virtual UniquePtr<ImagePixelLayout>                                                                       \
  ConvertFrom(Utils_YUV420P*, const uint8_t*, const ImagePixelLayout*, uint8_t*) override; \
                                                                                                            \
  virtual UniquePtr<ImagePixelLayout>                                                                             \
  ConvertFrom(Utils_YUV420SP_NV12*, const uint8_t*, const ImagePixelLayout*, uint8_t*) override; \
                                                                                                                  \
  virtual UniquePtr<ImagePixelLayout>                                                                             \
  ConvertFrom(Utils_YUV420SP_NV21*, const uint8_t*, const ImagePixelLayout*, uint8_t*) override; \
                                                                                                                  \
  virtual UniquePtr<ImagePixelLayout>                                                                   \
  ConvertFrom(Utils_HSV*, const uint8_t*, const ImagePixelLayout*, uint8_t*) override; \
                                                                                                        \
  virtual UniquePtr<ImagePixelLayout>                                                                   \
  ConvertFrom(Utils_Lab*, const uint8_t*, const ImagePixelLayout*, uint8_t*) override; \
                                                                                                        \
  virtual UniquePtr<ImagePixelLayout>                                                                     \
  ConvertFrom(Utils_Depth*, const uint8_t*, const ImagePixelLayout*, uint8_t*) override; \
                                                                                                          \
  virtual bool                                  \
  CanConvertFrom(ImageBitmapFormat) override;   \
};

DECLARE_Utils(RGBA32)
DECLARE_Utils(BGRA32)
DECLARE_Utils(RGB24)
DECLARE_Utils(BGR24)
DECLARE_Utils(Gray8)
DECLARE_Utils(YUV444P)
DECLARE_Utils(YUV422P)
DECLARE_Utils(YUV420P)
DECLARE_Utils(YUV420SP_NV12)
DECLARE_Utils(YUV420SP_NV21)
DECLARE_Utils(HSV)
DECLARE_Utils(Lab)
DECLARE_Utils(Depth)

#undef DECLARE_Utils

/*
 * ImageBitmapFormatUtils.
 */
/* static */ UtilsUniquePtr
Utils::GetUtils(ImageBitmapFormat aFormat)
{
  switch(aFormat)
  {
  case ImageBitmapFormat::RGBA32:
    return UtilsUniquePtr(&Utils_RGBA32::GetInstance());
  case ImageBitmapFormat::BGRA32:
    return UtilsUniquePtr(&Utils_BGRA32::GetInstance());
  case ImageBitmapFormat::RGB24:
    return UtilsUniquePtr(&Utils_RGB24::GetInstance());
  case ImageBitmapFormat::BGR24:
    return UtilsUniquePtr(&Utils_BGR24::GetInstance());
  case ImageBitmapFormat::GRAY8:
    return UtilsUniquePtr(&Utils_Gray8::GetInstance());
  case ImageBitmapFormat::YUV444P:
    return UtilsUniquePtr(&Utils_YUV444P::GetInstance());
  case ImageBitmapFormat::YUV422P:
    return UtilsUniquePtr(&Utils_YUV422P::GetInstance());
  case ImageBitmapFormat::YUV420P:
    return UtilsUniquePtr(&Utils_YUV420P::GetInstance());
  case ImageBitmapFormat::YUV420SP_NV12:
    return UtilsUniquePtr(&Utils_YUV420SP_NV12::GetInstance());
  case ImageBitmapFormat::YUV420SP_NV21:
    return UtilsUniquePtr(&Utils_YUV420SP_NV21::GetInstance());
  case ImageBitmapFormat::HSV:
    return UtilsUniquePtr(&Utils_HSV::GetInstance());
  case ImageBitmapFormat::Lab:
    return UtilsUniquePtr(&Utils_Lab::GetInstance());
  case ImageBitmapFormat::DEPTH:
    return UtilsUniquePtr(&Utils_Depth::GetInstance());
  default:
    return nullptr;
  }
}

/*
 * Helper functions.
 */
template<typename SrcType, typename DstType>
static UniquePtr<ImagePixelLayout>
CvtSimpleImgToSimpleImg(Utils* aSrcUtils, const SrcType* aSrcBuffer,
                        const ImagePixelLayout* aSrcLayout, DstType* aDstBuffer,
                        ImageBitmapFormat aDstFormat, int aDstChannelCount,
                        std::function<int (const SrcType*, int, DstType*, int, int, int)> converter)
{
  MOZ_ASSERT(aSrcUtils, "Convert color from a null utility object.");
  MOZ_ASSERT(aSrcBuffer, "Convert color from a null buffer.");
  MOZ_ASSERT(aSrcLayout, "Convert color from a null layout.");
  MOZ_ASSERT(aDstBuffer, "Convert color to a null buffer.");

  const nsTArray<ChannelPixelLayout>& channels = *aSrcLayout;
  MOZ_ASSERT(channels.Length() == aSrcUtils->GetChannelCount(),
             "The channel count is wrong.");

  const int dstStride = channels[0].mWidth * aDstChannelCount * sizeof(DstType);
  int rv = converter(aSrcBuffer, channels[0].mStride,
                     aDstBuffer, dstStride,
                     channels[0].mWidth, channels[0].mHeight);

  if (NS_WARN_IF(rv != 0)) {
    return nullptr;
  }

  return CreateDefaultPixelLayout(aDstFormat, channels[0].mWidth,
                                  channels[0].mHeight, dstStride);
}

static UniquePtr<ImagePixelLayout>
CvtYUVImgToSimpleImg(Utils* aSrcUtils, const uint8_t* aSrcBuffer,
                     const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer,
                     ImageBitmapFormat aDstFormat, int aDstChannelCount,
                     std::function<int (const uint8_t*, int, const uint8_t*, int, const uint8_t*, int, uint8_t*, int, int, int)> converter)
{
  MOZ_ASSERT(aSrcUtils, "Convert color from a null utility object.");
  MOZ_ASSERT(aSrcBuffer, "Convert color from a null buffer.");
  MOZ_ASSERT(aSrcLayout, "Convert color from a null layout.");
  MOZ_ASSERT(aDstBuffer, "Convert color to a null buffer.");

  const nsTArray<ChannelPixelLayout>& channels = *aSrcLayout;
  MOZ_ASSERT(channels.Length() == aSrcUtils->GetChannelCount(),
             "The channel count is wrong.");

  const int dstStride = channels[0].mWidth * aDstChannelCount * sizeof(uint8_t);
  int rv = converter(aSrcBuffer + channels[0].mOffset, channels[0].mStride,
                     aSrcBuffer + channels[1].mOffset, channels[1].mStride,
                     aSrcBuffer + channels[2].mOffset, channels[2].mStride,
                     aDstBuffer, dstStride,
                     channels[0].mWidth, channels[0].mHeight);

  if (NS_WARN_IF(rv != 0)) {
    return nullptr;
  }

  return CreateDefaultPixelLayout(aDstFormat, channels[0].mWidth,
                                  channels[0].mHeight, dstStride);
}

static UniquePtr<ImagePixelLayout>
CvtNVImgToSimpleImg(Utils* aSrcUtils, const uint8_t* aSrcBuffer,
                    const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer,
                    ImageBitmapFormat aDstFormat, int aDstChannelCount,
                    std::function<int (const uint8_t*, int, const uint8_t*, int, uint8_t*, int, int, int)> converter)
{
  MOZ_ASSERT(aSrcUtils, "Convert color from a null utility object.");
  MOZ_ASSERT(aSrcBuffer, "Convert color from a null buffer.");
  MOZ_ASSERT(aSrcLayout, "Convert color from a null layout.");
  MOZ_ASSERT(aDstBuffer, "Convert color to a null buffer.");

  const nsTArray<ChannelPixelLayout>& channels = *aSrcLayout;
  MOZ_ASSERT(channels.Length() == aSrcUtils->GetChannelCount(),
             "The channel count is wrong.");

  const int dstStride = channels[0].mWidth * aDstChannelCount * sizeof(uint8_t);
  int rv = converter(aSrcBuffer + channels[0].mOffset, channels[0].mStride,
                     aSrcBuffer + channels[1].mOffset, channels[1].mStride,
                     aDstBuffer, dstStride,
                     channels[0].mWidth, channels[0].mHeight);

  if (NS_WARN_IF(rv != 0)) {
    return nullptr;
  }

  return CreateDefaultPixelLayout(aDstFormat, channels[0].mWidth,
                                  channels[0].mHeight, dstStride);
}

static UniquePtr<ImagePixelLayout>
CvtSimpleImgToYUVImg(Utils* aSrcUtils, const uint8_t* aSrcBuffer,
                     const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer,
                     ImageBitmapFormat aDstFormat,
                     std::function<int (const uint8_t*, int, uint8_t*, int, uint8_t*, int, uint8_t*, int, int, int)> converter)
{
  MOZ_ASSERT(aSrcUtils, "Convert color from a null utility object.");
  MOZ_ASSERT(aSrcBuffer, "Convert color from a null buffer.");
  MOZ_ASSERT(aSrcLayout, "Convert color from a null layout.");
  MOZ_ASSERT(aDstBuffer, "Convert color to a null buffer.");

  UniquePtr<ImagePixelLayout> layout =
    CreateDefaultPixelLayout(aDstFormat, (*aSrcLayout)[0].mWidth,
                             (*aSrcLayout)[0].mHeight, (*aSrcLayout)[0].mWidth);

  MOZ_ASSERT(layout, "Cannot create a ImagePixelLayout.");

  const nsTArray<ChannelPixelLayout>& channels = *layout;

  int rv = converter(aSrcBuffer, (*aSrcLayout)[0].mStride,
                     aDstBuffer + channels[0].mOffset, channels[0].mStride,
                     aDstBuffer + channels[1].mOffset, channels[1].mStride,
                     aDstBuffer + channels[2].mOffset, channels[2].mStride,
                     channels[0].mWidth, channels[0].mHeight);

  if (NS_WARN_IF(rv != 0)) {
    return nullptr;
  }

  return layout;
}

static UniquePtr<ImagePixelLayout>
CvtSimpleImgToNVImg(Utils* aSrcUtils, const uint8_t* aSrcBuffer,
                    const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer,
                    ImageBitmapFormat aDstFormat,
                    std::function<int (const uint8_t*, int, uint8_t*, int, uint8_t*, int, int, int)> converter)
{
  MOZ_ASSERT(aSrcUtils, "Convert color from a null utility object.");
  MOZ_ASSERT(aSrcBuffer, "Convert color from a null buffer.");
  MOZ_ASSERT(aSrcLayout, "Convert color from a null layout.");
  MOZ_ASSERT(aDstBuffer, "Convert color to a null buffer.");

  UniquePtr<ImagePixelLayout> layout =
    CreateDefaultPixelLayout(aDstFormat, (*aSrcLayout)[0].mWidth,
                             (*aSrcLayout)[0].mHeight, (*aSrcLayout)[0].mWidth);

  MOZ_ASSERT(layout, "Cannot create a ImagePixelLayout.");

  const nsTArray<ChannelPixelLayout>& channels = *layout;

  int rv = converter(aSrcBuffer, (*aSrcLayout)[0].mStride,
                     aDstBuffer + channels[0].mOffset, channels[0].mStride,
                     aDstBuffer + channels[1].mOffset, channels[1].mStride,
                     channels[0].mWidth, channels[0].mHeight);

  if (NS_WARN_IF(rv != 0)) {
    return nullptr;
  }

  return layout;
}

template<class SrcUtilsType, class DstUtilsType>
static UniquePtr<ImagePixelLayout>
TwoPassConversion(SrcUtilsType* aSrcUtils, const uint8_t* aSrcBuffer,
                  const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer,
                  ImageBitmapFormat aMiddleFormat, DstUtilsType* aDstUtils)
{
  MOZ_ASSERT(aSrcUtils, "Convert color from a null source utility.");
  MOZ_ASSERT(aSrcBuffer, "Convert color from a null buffer.");
  MOZ_ASSERT(aSrcLayout, "Convert color from a null layout.");
  MOZ_ASSERT(aDstBuffer, "Convert color to a null buffer.");

  // I444 -> I420 -> I422
  UtilsUniquePtr yuv420PUtils = Utils::GetUtils(aMiddleFormat);
  UniquePtr<uint8_t> yuv420PBuffer(new uint8_t[yuv420PUtils->NeededBufferSize((*aSrcLayout)[0].mWidth, (*aSrcLayout)[0].mHeight)]);
  UniquePtr<ImagePixelLayout> yuv420PLayout = yuv420PUtils->ConvertFrom(aSrcUtils, aSrcBuffer, aSrcLayout, yuv420PBuffer.get());
  return yuv420PUtils->ConvertTo(aDstUtils, yuv420PBuffer.get(), yuv420PLayout.get(), aDstBuffer);
}

static UniquePtr<ImagePixelLayout>
PureCopy(Utils* aSrcUtils, const uint8_t* aSrcBuffer,
         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer,
         ImageBitmapFormat aDstFormat)
{
  MOZ_ASSERT(aSrcUtils, "Convert color from a null utility object.");
  MOZ_ASSERT(aSrcBuffer, "Convert color from a null buffer.");
  MOZ_ASSERT(aSrcLayout, "Convert color from a null layout.");
  MOZ_ASSERT(aDstBuffer, "Convert color to a null buffer.");

  const nsTArray<ChannelPixelLayout>& channels = *aSrcLayout;
  MOZ_ASSERT(channels.Length() == aSrcUtils->GetChannelCount(),
             "The channel count is wrong.");


  uint32_t length = 0;

  if (aDstFormat == ImageBitmapFormat::RGBA32 ||
      aDstFormat == ImageBitmapFormat::BGRA32 ||
      aDstFormat == ImageBitmapFormat::RGB24 ||
      aDstFormat == ImageBitmapFormat::BGR24 ||
      aDstFormat == ImageBitmapFormat::GRAY8 ||
      aDstFormat == ImageBitmapFormat::HSV ||
      aDstFormat == ImageBitmapFormat::Lab ||
      aDstFormat == ImageBitmapFormat::DEPTH) {
    length = channels[0].mHeight * channels[0].mStride;
  } else if (aDstFormat == ImageBitmapFormat::YUV444P ||
             aDstFormat == ImageBitmapFormat::YUV422P ||
             aDstFormat == ImageBitmapFormat::YUV420P) {
    length = channels[0].mHeight * channels[0].mStride +
             channels[1].mHeight * channels[1].mStride +
             channels[2].mHeight * channels[2].mStride;
  } else if (aDstFormat == ImageBitmapFormat::YUV420SP_NV12 ||
             aDstFormat == ImageBitmapFormat::YUV420SP_NV21) {
    length = channels[0].mHeight * channels[0].mStride +
             channels[1].mHeight * channels[1].mStride;
  }

  memcpy(aDstBuffer, aSrcBuffer, length);

  UniquePtr<ImagePixelLayout> layout(new ImagePixelLayout(*aSrcLayout));
  return layout;
}

UniquePtr<ImagePixelLayout>
CreateDefaultLayoutForSimpleImage(uint32_t aWidth, uint32_t aHeight,
                                  uint32_t aStride, int aChannels,
                                  int aBytesPerPixelValue,
                                  ChannelPixelLayoutDataType aDataType)
{
  UniquePtr<ImagePixelLayout> layout(new ImagePixelLayout(aChannels));

  // set mChannels
  for (uint8_t i = 0; i < aChannels; ++i) {
    ChannelPixelLayout* channel = layout->AppendElement();
    channel->mOffset = i * aBytesPerPixelValue;
    channel->mWidth = aWidth;
    channel->mHeight = aHeight;
    channel->mDataType = aDataType; //ChannelPixelLayoutDataType::Uint8;
    channel->mStride = aStride;
    channel->mSkip = aChannels - 1;
  }

  return layout;
}

/*
 * Utils_RGBA32.
 */
/* static */Utils_RGBA32&
Utils_RGBA32::GetInstance()
{
  static Utils_RGBA32 instance;
  return instance;
}

Utils_RGBA32::Utils_RGBA32()
: Utils(4, ChannelPixelLayoutDataType::Uint8)
{
}

uint32_t
Utils_RGBA32::NeededBufferSize(uint32_t aWidth, uint32_t aHeight)
{
  return aWidth * aHeight * (uint32_t)mChannels * mBytesPerPixelValue;
}

UniquePtr<ImagePixelLayout>
Utils_RGBA32::ConvertTo(Utils* aDstFormat, const uint8_t* aSrcBuffer,
                        const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return aDstFormat->ConvertFrom(this, aSrcBuffer, aSrcLayout, aDstBuffer);
}

UniquePtr<ImagePixelLayout>
Utils_RGBA32::ConvertFrom(Utils_RGBA32* aSrcUtils, const uint8_t* aSrcBuffer,
                          const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return PureCopy(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::RGBA32);
}

UniquePtr<ImagePixelLayout>
Utils_RGBA32::ConvertFrom(Utils_BGRA32* aSrcFormat, const uint8_t* aSrcBuffer,
                          const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToSimpleImg<uint8_t, uint8_t>(aSrcFormat, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::RGBA32, 4, &libyuv::ABGRToARGB);
}

UniquePtr<ImagePixelLayout>
Utils_RGBA32::ConvertFrom(Utils_RGB24* aSrcFormat, const uint8_t* aSrcBuffer,
                          const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToSimpleImg<uint8_t, uint8_t>(aSrcFormat, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::RGBA32, 4, &RGB24ToRGBA32);
}

UniquePtr<ImagePixelLayout>
Utils_RGBA32::ConvertFrom(Utils_BGR24* aSrcFormat, const uint8_t* aSrcBuffer,
                          const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToSimpleImg<uint8_t, uint8_t>(aSrcFormat, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::RGBA32, 4, &BGR24ToRGBA32);
}

UniquePtr<ImagePixelLayout>
Utils_RGBA32::ConvertFrom(Utils_Gray8* aSrcFormat, const uint8_t* aSrcBuffer,
                          const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return nullptr;
}

UniquePtr<ImagePixelLayout>
Utils_RGBA32::ConvertFrom(Utils_YUV444P* aSrcUtils, const uint8_t* aSrcBuffer,
                          const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtYUVImgToSimpleImg(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::RGBA32, 4, &YUV444PToRGBA32);
}

UniquePtr<ImagePixelLayout>
Utils_RGBA32::ConvertFrom(Utils_YUV422P* aSrcUtils, const uint8_t* aSrcBuffer,
                          const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtYUVImgToSimpleImg(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::RGBA32, 4, &YUV422PToRGBA32);
}

UniquePtr<ImagePixelLayout>
Utils_RGBA32::ConvertFrom(Utils_YUV420P* aSrcUtils, const uint8_t* aSrcBuffer,
                          const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtYUVImgToSimpleImg(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::RGBA32, 4, &libyuv::I420ToABGR);
}

UniquePtr<ImagePixelLayout>
Utils_RGBA32::ConvertFrom(Utils_YUV420SP_NV12* aSrcUtils, const uint8_t* aSrcBuffer,
                          const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtNVImgToSimpleImg(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::RGBA32, 4, &NV12ToRGBA32);
}

UniquePtr<ImagePixelLayout>
Utils_RGBA32::ConvertFrom(Utils_YUV420SP_NV21* aSrcUtils, const uint8_t* aSrcBuffer,
                          const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtNVImgToSimpleImg(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::RGBA32, 4, &NV21ToRGBA32);
}

UniquePtr<ImagePixelLayout>
Utils_RGBA32::ConvertFrom(Utils_HSV* aSrcFormat, const uint8_t* aSrcBuffer,
                          const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToSimpleImg<float, uint8_t>(aSrcFormat, (const float*)aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::RGBA32, 4, &HSVToRGBA32);
}

UniquePtr<ImagePixelLayout>
Utils_RGBA32::ConvertFrom(Utils_Lab* aSrcFormat, const uint8_t* aSrcBuffer,
                          const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToSimpleImg<float, uint8_t>(aSrcFormat, (const float*)aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::RGBA32, 4, &LabToRGBA32);
}

UniquePtr<ImagePixelLayout>
Utils_RGBA32::ConvertFrom(Utils_Depth* aSrcFormat, const uint8_t* aSrcBuffer,
                          const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return nullptr;
}

bool
Utils_RGBA32::CanConvertFrom(ImageBitmapFormat aSrcFormat)
{
  if (aSrcFormat == ImageBitmapFormat::GRAY8 ||
      aSrcFormat == ImageBitmapFormat::DEPTH) {
    return false;
  }

  return true;
}

UniquePtr<ImagePixelLayout>
Utils_RGBA32::CreateDefaultLayout(uint32_t aWidth, uint32_t aHeight, uint32_t aStride)
{
  return CreateDefaultLayoutForSimpleImage(aWidth, aHeight, aStride, mChannels, mBytesPerPixelValue, mDataType);
}

/*
 * Utils_BGRA32.
 */
/* static */Utils_BGRA32&
Utils_BGRA32::GetInstance()
{
  static Utils_BGRA32 instance;
  return instance;
}

Utils_BGRA32::Utils_BGRA32()
: Utils(4, ChannelPixelLayoutDataType::Uint8)
{
}

uint32_t
Utils_BGRA32::NeededBufferSize(uint32_t aWidth, uint32_t aHeight)
{
  return aWidth * aHeight * (uint32_t)mChannels * mBytesPerPixelValue;
}

UniquePtr<ImagePixelLayout>
Utils_BGRA32::ConvertTo(Utils* aDstFormat, const uint8_t* aSrcBuffer,
                        const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return aDstFormat->ConvertFrom(this, aSrcBuffer, aSrcLayout, aDstBuffer);
}

UniquePtr<ImagePixelLayout>
Utils_BGRA32::ConvertFrom(Utils_RGBA32* aSrcFormat, const uint8_t* aSrcBuffer,
                          const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToSimpleImg<uint8_t, uint8_t>(aSrcFormat, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::BGRA32, 4, &libyuv::ABGRToARGB);
}

UniquePtr<ImagePixelLayout>
Utils_BGRA32::ConvertFrom(Utils_BGRA32* aSrcUtils, const uint8_t* aSrcBuffer,
                          const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return PureCopy(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::BGRA32);
}

UniquePtr<ImagePixelLayout>
Utils_BGRA32::ConvertFrom(Utils_RGB24* aSrcFormat, const uint8_t* aSrcBuffer,
                          const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToSimpleImg<uint8_t, uint8_t>(aSrcFormat, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::BGRA32, 4, &RGB24ToBGRA32);
}

UniquePtr<ImagePixelLayout>
Utils_BGRA32::ConvertFrom(Utils_BGR24* aSrcFormat, const uint8_t* aSrcBuffer,
                          const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToSimpleImg<uint8_t, uint8_t>(aSrcFormat, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::BGRA32, 4, &BGR24ToBGRA32);
}

UniquePtr<ImagePixelLayout>
Utils_BGRA32::ConvertFrom(Utils_Gray8* aSrcFormat, const uint8_t* aSrcBuffer,
                          const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return nullptr;
}

UniquePtr<ImagePixelLayout>
Utils_BGRA32::ConvertFrom(Utils_YUV444P* aSrcUtils, const uint8_t* aSrcBuffer,
                          const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtYUVImgToSimpleImg(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::BGRA32, 4, &YUV444PToBGRA32);
}

UniquePtr<ImagePixelLayout>
Utils_BGRA32::ConvertFrom(Utils_YUV422P* aSrcUtils, const uint8_t* aSrcBuffer,
                          const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtYUVImgToSimpleImg(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::BGRA32, 4, &libyuv::I422ToARGB);
}

UniquePtr<ImagePixelLayout>
Utils_BGRA32::ConvertFrom(Utils_YUV420P* aSrcUtils, const uint8_t* aSrcBuffer,
                          const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtYUVImgToSimpleImg(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::BGRA32, 4, &libyuv::I420ToARGB);
}

UniquePtr<ImagePixelLayout>
Utils_BGRA32::ConvertFrom(Utils_YUV420SP_NV12* aSrcUtils, const uint8_t* aSrcBuffer,
                          const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtNVImgToSimpleImg(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::BGRA32, 4, &libyuv::NV12ToARGB);
}

UniquePtr<ImagePixelLayout>
Utils_BGRA32::ConvertFrom(Utils_YUV420SP_NV21* aSrcUtils, const uint8_t* aSrcBuffer,
                          const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtNVImgToSimpleImg(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::BGRA32, 4, &libyuv::NV21ToARGB);
}

UniquePtr<ImagePixelLayout>
Utils_BGRA32::ConvertFrom(Utils_HSV* aSrcFormat, const uint8_t* aSrcBuffer,
                          const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToSimpleImg<float, uint8_t>(aSrcFormat, (const float*)aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::BGRA32, 4, &HSVToBGRA32);
}

UniquePtr<ImagePixelLayout>
Utils_BGRA32::ConvertFrom(Utils_Lab* aSrcFormat, const uint8_t* aSrcBuffer,
                          const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToSimpleImg<float, uint8_t>(aSrcFormat, (const float*)aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::BGRA32, 4, &LabToBGRA32);
}

UniquePtr<ImagePixelLayout>
Utils_BGRA32::ConvertFrom(Utils_Depth* aSrcFormat, const uint8_t* aSrcBuffer,
                          const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return nullptr;
}

bool
Utils_BGRA32::CanConvertFrom(ImageBitmapFormat aSrcFormat)
{
  if (aSrcFormat == ImageBitmapFormat::RGBA32 ||
      aSrcFormat == ImageBitmapFormat::BGRA32 ||
      aSrcFormat == ImageBitmapFormat::YUV420P) {
    return true;
  }

  return false;
}

UniquePtr<ImagePixelLayout>
Utils_BGRA32::CreateDefaultLayout(uint32_t aWidth, uint32_t aHeight, uint32_t aStride)
{
  return CreateDefaultLayoutForSimpleImage(aWidth, aHeight, aStride, mChannels, mBytesPerPixelValue, mDataType);
}

/*
 * Utils_RGB24.
 */
/* static */Utils_RGB24&
Utils_RGB24::GetInstance()
{
  static Utils_RGB24 instance;
  return instance;
}

Utils_RGB24::Utils_RGB24()
: Utils(3, ChannelPixelLayoutDataType::Uint8)
{
}

uint32_t
Utils_RGB24::NeededBufferSize(uint32_t aWidth, uint32_t aHeight)
{
  return aWidth * aHeight * (uint32_t)mChannels * mBytesPerPixelValue;
}

UniquePtr<ImagePixelLayout>
Utils_RGB24::ConvertTo(Utils* aDstFormat, const uint8_t* aSrcBuffer,
                       const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return aDstFormat->ConvertFrom(this, aSrcBuffer, aSrcLayout, aDstBuffer);
}

UniquePtr<ImagePixelLayout>
Utils_RGB24::ConvertFrom(Utils_RGBA32* aSrcFormat, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToSimpleImg<uint8_t, uint8_t>(aSrcFormat, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::RGB24, 3, &RGBA32ToRGB24);
}

UniquePtr<ImagePixelLayout>
Utils_RGB24::ConvertFrom(Utils_BGRA32* aSrcFormat, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToSimpleImg<uint8_t, uint8_t>(aSrcFormat, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::RGB24, 3, &BGRA32ToRGB24);
}

UniquePtr<ImagePixelLayout>
Utils_RGB24::ConvertFrom(Utils_RGB24* aSrcUtils, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return PureCopy(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::RGB24);
}

UniquePtr<ImagePixelLayout>
Utils_RGB24::ConvertFrom(Utils_BGR24* aSrcUtils, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToSimpleImg<uint8_t, uint8_t>(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::RGB24, 3, &BGR24ToRGB24);
}

UniquePtr<ImagePixelLayout>
Utils_RGB24::ConvertFrom(Utils_Gray8* aSrcFormat, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return nullptr;
}

UniquePtr<ImagePixelLayout>
Utils_RGB24::ConvertFrom(Utils_YUV444P* aSrcUtils, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtYUVImgToSimpleImg(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::RGB24, 3, &YUV444PToRGB24);
}

UniquePtr<ImagePixelLayout>
Utils_RGB24::ConvertFrom(Utils_YUV422P* aSrcUtils, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtYUVImgToSimpleImg(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::RGB24, 3, &YUV422PToRGB24);
}

UniquePtr<ImagePixelLayout>
Utils_RGB24::ConvertFrom(Utils_YUV420P* aSrcUtils, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtYUVImgToSimpleImg(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::RGB24, 3, &YUV420PToRGB24);
}

UniquePtr<ImagePixelLayout>
Utils_RGB24::ConvertFrom(Utils_YUV420SP_NV12* aSrcUtils, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtNVImgToSimpleImg(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::RGB24, 3, &NV12ToRGB24);
}

UniquePtr<ImagePixelLayout>
Utils_RGB24::ConvertFrom(Utils_YUV420SP_NV21* aSrcUtils, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtNVImgToSimpleImg(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::RGB24, 3, &NV21ToRGB24);
}

UniquePtr<ImagePixelLayout>
Utils_RGB24::ConvertFrom(Utils_HSV* aSrcFormat, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToSimpleImg<float, uint8_t>(aSrcFormat, (const float*)aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::RGB24, 3, &HSVToRGB24);
}

UniquePtr<ImagePixelLayout>
Utils_RGB24::ConvertFrom(Utils_Lab* aSrcFormat, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToSimpleImg<float, uint8_t>(aSrcFormat, (const float*)aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::RGB24, 3, &LabToRGB24);
}

UniquePtr<ImagePixelLayout>
Utils_RGB24::ConvertFrom(Utils_Depth* aSrcFormat, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return nullptr;
}

bool
Utils_RGB24::CanConvertFrom(ImageBitmapFormat aSrcFormat)
{
  if (aSrcFormat == ImageBitmapFormat::GRAY8 ||
      aSrcFormat == ImageBitmapFormat::DEPTH) {
    return false;
  }

  return true;
}

UniquePtr<ImagePixelLayout>
Utils_RGB24::CreateDefaultLayout(uint32_t aWidth, uint32_t aHeight, uint32_t aStride)
{
  return CreateDefaultLayoutForSimpleImage(aWidth, aHeight, aStride, mChannels, mBytesPerPixelValue, mDataType);
}

/*
 * Utils_BGR24.
 */
/* static */Utils_BGR24&
Utils_BGR24::GetInstance()
{
  static Utils_BGR24 instance;
  return instance;
}

Utils_BGR24::Utils_BGR24()
: Utils(3, ChannelPixelLayoutDataType::Uint8)
{
}

uint32_t
Utils_BGR24::NeededBufferSize(uint32_t aWidth, uint32_t aHeight)
{
  return aWidth * aHeight * (uint32_t)mChannels * mBytesPerPixelValue;
}

UniquePtr<ImagePixelLayout>
Utils_BGR24::ConvertTo(Utils* aDstFormat, const uint8_t* aSrcBuffer,
                       const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return aDstFormat->ConvertFrom(this, aSrcBuffer, aSrcLayout, aDstBuffer);
}

UniquePtr<ImagePixelLayout>
Utils_BGR24::ConvertFrom(Utils_RGBA32* aSrcFormat, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToSimpleImg<uint8_t, uint8_t>(aSrcFormat, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::BGR24, 3, &RGBA32ToBGR24);
}

UniquePtr<ImagePixelLayout>
Utils_BGR24::ConvertFrom(Utils_BGRA32* aSrcFormat, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToSimpleImg<uint8_t, uint8_t>(aSrcFormat, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::BGR24, 3, &BGRA32ToBGR24);
}

UniquePtr<ImagePixelLayout>
Utils_BGR24::ConvertFrom(Utils_RGB24* aSrcUtils, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToSimpleImg<uint8_t, uint8_t>(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::BGR24, 3, &RGB24ToBGR24);
}

UniquePtr<ImagePixelLayout>
Utils_BGR24::ConvertFrom(Utils_BGR24* aSrcUtils, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return PureCopy(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::BGR24);
}

UniquePtr<ImagePixelLayout>
Utils_BGR24::ConvertFrom(Utils_Gray8* aSrcFormat, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return nullptr;
}

UniquePtr<ImagePixelLayout>
Utils_BGR24::ConvertFrom(Utils_YUV444P* aSrcUtils, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtYUVImgToSimpleImg(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::BGR24, 3, &YUV444PToBGR24);
}

UniquePtr<ImagePixelLayout>
Utils_BGR24::ConvertFrom(Utils_YUV422P* aSrcUtils, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtYUVImgToSimpleImg(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::BGR24, 3, &YUV422PToBGR24);
}

UniquePtr<ImagePixelLayout>
Utils_BGR24::ConvertFrom(Utils_YUV420P* aSrcUtils, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtYUVImgToSimpleImg(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::BGR24, 3, &YUV420PToBGR24);
}

UniquePtr<ImagePixelLayout>
Utils_BGR24::ConvertFrom(Utils_YUV420SP_NV12* aSrcUtils, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtNVImgToSimpleImg(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::BGR24, 3, &NV12ToBGR24);
}

UniquePtr<ImagePixelLayout>
Utils_BGR24::ConvertFrom(Utils_YUV420SP_NV21* aSrcUtils, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtNVImgToSimpleImg(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::BGR24, 3, &NV21ToBGR24);
}

UniquePtr<ImagePixelLayout>
Utils_BGR24::ConvertFrom(Utils_HSV* aSrcFormat, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToSimpleImg<float, uint8_t>(aSrcFormat, (const float*)aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::BGR24, 3, &HSVToBGR24);
}

UniquePtr<ImagePixelLayout>
Utils_BGR24::ConvertFrom(Utils_Lab* aSrcFormat, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToSimpleImg<float, uint8_t>(aSrcFormat, (const float*)aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::BGR24, 3, &LabToBGR24);
}

UniquePtr<ImagePixelLayout>
Utils_BGR24::ConvertFrom(Utils_Depth* aSrcFormat, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return nullptr;
}

bool
Utils_BGR24::CanConvertFrom(ImageBitmapFormat aSrcFormat)
{
  if (aSrcFormat == ImageBitmapFormat::GRAY8 ||
      aSrcFormat == ImageBitmapFormat::DEPTH) {
    return false;
  }

  return true;
}

UniquePtr<ImagePixelLayout>
Utils_BGR24::CreateDefaultLayout(uint32_t aWidth, uint32_t aHeight, uint32_t aStride)
{
  return CreateDefaultLayoutForSimpleImage(aWidth, aHeight, aStride, mChannels, mBytesPerPixelValue, mDataType);
}

/*
 * Utils_Gray8.
 */
/* static */Utils_Gray8&
Utils_Gray8::GetInstance()
{
  static Utils_Gray8 instance;
  return instance;
}

Utils_Gray8::Utils_Gray8()
: Utils(1, ChannelPixelLayoutDataType::Uint8)
{
}

uint32_t
Utils_Gray8::NeededBufferSize(uint32_t aWidth, uint32_t aHeight)
{
  return aWidth * aHeight * (uint32_t)mChannels * mBytesPerPixelValue;
}

UniquePtr<ImagePixelLayout>
Utils_Gray8::ConvertTo(Utils* aDstFormat, const uint8_t* aSrcBuffer,
                       const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return aDstFormat->ConvertFrom(this, aSrcBuffer, aSrcLayout, aDstBuffer);
}

UniquePtr<ImagePixelLayout>
Utils_Gray8::ConvertFrom(Utils_RGBA32* aSrcFormat, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToSimpleImg<uint8_t, uint8_t>(aSrcFormat, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::GRAY8, 1, &RGBA32ToGray8);
}

UniquePtr<ImagePixelLayout>
Utils_Gray8::ConvertFrom(Utils_BGRA32* aSrcFormat, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToSimpleImg<uint8_t, uint8_t>(aSrcFormat, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::GRAY8, 1, &BGRA32ToGray8);
}

UniquePtr<ImagePixelLayout>
Utils_Gray8::ConvertFrom(Utils_RGB24* aSrcUtils, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToSimpleImg<uint8_t, uint8_t>(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::GRAY8, 1, &RGB24ToGray8);
}

UniquePtr<ImagePixelLayout>
Utils_Gray8::ConvertFrom(Utils_BGR24* aSrcUtils, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToSimpleImg<uint8_t, uint8_t>(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::GRAY8, 1, &BGR24ToGray8);
}

UniquePtr<ImagePixelLayout>
Utils_Gray8::ConvertFrom(Utils_Gray8* aSrcUtils, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return PureCopy(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::GRAY8);
}

UniquePtr<ImagePixelLayout>
Utils_Gray8::ConvertFrom(Utils_YUV444P* aSrcUtils, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtYUVImgToSimpleImg(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::GRAY8, 1, &YUV444PToGray8);
}

UniquePtr<ImagePixelLayout>
Utils_Gray8::ConvertFrom(Utils_YUV422P* aSrcUtils, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtYUVImgToSimpleImg(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::GRAY8, 1, &YUV422PToGray8);
}

UniquePtr<ImagePixelLayout>
Utils_Gray8::ConvertFrom(Utils_YUV420P* aSrcUtils, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtYUVImgToSimpleImg(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::GRAY8, 1, &YUV420PToGray8);
}

UniquePtr<ImagePixelLayout>
Utils_Gray8::ConvertFrom(Utils_YUV420SP_NV12* aSrcUtils, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtNVImgToSimpleImg(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::GRAY8, 1, &NV12ToGray8);
}

UniquePtr<ImagePixelLayout>
Utils_Gray8::ConvertFrom(Utils_YUV420SP_NV21* aSrcUtils, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtNVImgToSimpleImg(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::GRAY8, 1, &NV21ToGray8);
}

UniquePtr<ImagePixelLayout>
Utils_Gray8::ConvertFrom(Utils_HSV* aSrcUtils, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return TwoPassConversion(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::RGB24, this);
}

UniquePtr<ImagePixelLayout>
Utils_Gray8::ConvertFrom(Utils_Lab* aSrcUtils, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return TwoPassConversion(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::RGB24, this);
}

UniquePtr<ImagePixelLayout>
Utils_Gray8::ConvertFrom(Utils_Depth* aSrcUtils, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return nullptr;
}

bool
Utils_Gray8::CanConvertFrom(ImageBitmapFormat aSrcFormat)
{
  if (aSrcFormat == ImageBitmapFormat::DEPTH) {
    return false;
  }

  return true;
}

UniquePtr<ImagePixelLayout>
Utils_Gray8::CreateDefaultLayout(uint32_t aWidth, uint32_t aHeight, uint32_t aStride)
{
  return CreateDefaultLayoutForSimpleImage(aWidth, aHeight, aStride, mChannels, mBytesPerPixelValue, mDataType);
}

/*
 * class Utils_YUV444P.
 */
/* static */Utils_YUV444P&
Utils_YUV444P::GetInstance()
{
  static Utils_YUV444P instance;
  return instance;
}

Utils_YUV444P::Utils_YUV444P()
: Utils(3, ChannelPixelLayoutDataType::Uint8)
{
}

uint32_t
Utils_YUV444P::NeededBufferSize(uint32_t aWidth, uint32_t aHeight)
{
  return aWidth * aHeight * (uint32_t)mChannels * mBytesPerPixelValue;
}

UniquePtr<ImagePixelLayout>
Utils_YUV444P::ConvertTo(Utils* aDstFormat, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return aDstFormat->ConvertFrom(this, aSrcBuffer, aSrcLayout, aDstBuffer);
}

UniquePtr<ImagePixelLayout>
Utils_YUV444P::ConvertFrom(Utils_RGBA32* aSrcUtils, const uint8_t* aSrcBuffer,
                           const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToYUVImg(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::YUV444P, &RGBA32ToYUV444P);
}

UniquePtr<ImagePixelLayout>
Utils_YUV444P::ConvertFrom(Utils_BGRA32* aSrcUtils, const uint8_t* aSrcBuffer,
                           const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToYUVImg(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::YUV444P, &libyuv::ARGBToI444);
}

UniquePtr<ImagePixelLayout>
Utils_YUV444P::ConvertFrom(Utils_RGB24* aSrcUtils, const uint8_t* aSrcBuffer,
                           const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToYUVImg(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::YUV444P, &RGB24ToYUV444P);
}

UniquePtr<ImagePixelLayout>
Utils_YUV444P::ConvertFrom(Utils_BGR24* aSrcUtils, const uint8_t* aSrcBuffer,
                           const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToYUVImg(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::YUV444P, &BGR24ToYUV444P);
}

UniquePtr<ImagePixelLayout>
Utils_YUV444P::ConvertFrom(Utils_Gray8* aSrcFormat, const uint8_t* aSrcBuffer,
                           const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return nullptr;
}

UniquePtr<ImagePixelLayout>
Utils_YUV444P::ConvertFrom(Utils_YUV444P* aSrcUtils, const uint8_t* aSrcBuffer,
                           const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return PureCopy(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::YUV444P);
}

// TODO: optimize me.
UniquePtr<ImagePixelLayout>
Utils_YUV444P::ConvertFrom(Utils_YUV422P* aSrcUtils, const uint8_t* aSrcBuffer,
                           const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return TwoPassConversion(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::YUV420P, this);
}

UniquePtr<ImagePixelLayout>
Utils_YUV444P::ConvertFrom(Utils_YUV420P*, const uint8_t* aSrcBuffer,
                           const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  MOZ_ASSERT(aSrcBuffer, "Convert color from a null buffer.");
  MOZ_ASSERT(aSrcLayout, "Convert color from a null layout.");
  MOZ_ASSERT(aDstBuffer, "Convert color to a null buffer.");

  UniquePtr<ImagePixelLayout> layout =
    CreateDefaultLayout((*aSrcLayout)[0].mWidth,
                        (*aSrcLayout)[0].mHeight,
                        (*aSrcLayout)[0].mWidth);

  MOZ_ASSERT(layout, "Cannot create a ImagePixelLayout of YUV444P");

  const nsTArray<ChannelPixelLayout>& channels = *layout;

  const nsTArray<ChannelPixelLayout>& srcChannels = *aSrcLayout;

  int rv = I420ToI444(aSrcBuffer + srcChannels[0].mOffset, srcChannels[0].mStride,
                      aSrcBuffer + srcChannels[1].mOffset, srcChannels[1].mStride,
                      aSrcBuffer + srcChannels[2].mOffset, srcChannels[2].mStride,
                      aDstBuffer + channels[0].mOffset, channels[0].mStride,
                      aDstBuffer + channels[1].mOffset, channels[1].mStride,
                      aDstBuffer + channels[2].mOffset, channels[2].mStride,
                      channels[0].mWidth, channels[0].mHeight);

  if (NS_WARN_IF(rv != 0)) {
    return nullptr;
  }

  return layout;
}

// TODO: optimize me.
UniquePtr<ImagePixelLayout>
Utils_YUV444P::ConvertFrom(Utils_YUV420SP_NV12* aSrcUtils, const uint8_t* aSrcBuffer,
                           const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return TwoPassConversion(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::YUV420P, this);
}

// TODO: optimize me.
UniquePtr<ImagePixelLayout>
Utils_YUV444P::ConvertFrom(Utils_YUV420SP_NV21* aSrcUtils, const uint8_t* aSrcBuffer,
                           const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return TwoPassConversion(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::YUV420P, this);
}

UniquePtr<ImagePixelLayout>
Utils_YUV444P::ConvertFrom(Utils_HSV* aSrcUtils, const uint8_t* aSrcBuffer,
                           const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return TwoPassConversion(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::RGB24, this);
}

UniquePtr<ImagePixelLayout>
Utils_YUV444P::ConvertFrom(Utils_Lab* aSrcUtils, const uint8_t* aSrcBuffer,
                           const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return TwoPassConversion(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::RGB24, this);
}

UniquePtr<ImagePixelLayout>
Utils_YUV444P::ConvertFrom(Utils_Depth* aSrcUtils, const uint8_t* aSrcBuffer,
                           const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return nullptr;
}

bool
Utils_YUV444P::CanConvertFrom(ImageBitmapFormat aSrcFormat)
{
  if (aSrcFormat == ImageBitmapFormat::GRAY8 ||
      aSrcFormat == ImageBitmapFormat::DEPTH) {
    return false;
  }

  return true;
}

UniquePtr<ImagePixelLayout>
Utils_YUV444P::CreateDefaultLayout(uint32_t aWidth, uint32_t aHeight, uint32_t aStride)
{
  UniquePtr<ImagePixelLayout> layout(new ImagePixelLayout(mChannels));

  // set mChannels
  ChannelPixelLayout* ychannel = layout->AppendElement();
  ChannelPixelLayout* uchannel = layout->AppendElement();
  ChannelPixelLayout* vchannel = layout->AppendElement();
  ychannel->mOffset = 0;
  ychannel->mWidth  = aWidth;
  ychannel->mHeight = aHeight;
  ychannel->mDataType = ChannelPixelLayoutDataType::Uint8;
  ychannel->mStride = aStride;
  ychannel->mSkip   = 0; // aYSkip;

  uchannel->mOffset = ychannel->mOffset + ychannel->mStride * ychannel->mHeight;
  uchannel->mWidth  = aWidth;
  uchannel->mHeight = aHeight;
  uchannel->mDataType = ChannelPixelLayoutDataType::Uint8;
  uchannel->mStride = aStride;
  uchannel->mSkip   = 0; // aUSkip;

  vchannel->mOffset = uchannel->mOffset + uchannel->mStride * uchannel->mHeight;
  vchannel->mWidth  = aWidth;
  vchannel->mHeight = aHeight;
  vchannel->mDataType = ChannelPixelLayoutDataType::Uint8;
  vchannel->mStride = aStride;
  vchannel->mSkip   = 0; // aVSkip;

  return layout;
}

/*
 * class Utils_YUV422P.
 */
/* static */Utils_YUV422P&
Utils_YUV422P::GetInstance()
{
  static Utils_YUV422P instance;
  return instance;
}

Utils_YUV422P::Utils_YUV422P()
: Utils(3, ChannelPixelLayoutDataType::Uint8)
{
}

uint32_t
Utils_YUV422P::NeededBufferSize(uint32_t aWidth, uint32_t aHeight)
{
  return aWidth * aHeight * mBytesPerPixelValue +
         2 * ((aWidth + 1) / 2) * aHeight * mBytesPerPixelValue;
}

UniquePtr<ImagePixelLayout>
Utils_YUV422P::ConvertTo(Utils* aDstFormat, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return aDstFormat->ConvertFrom(this, aSrcBuffer, aSrcLayout, aDstBuffer);
}

UniquePtr<ImagePixelLayout>
Utils_YUV422P::ConvertFrom(Utils_RGBA32* aSrcUtils, const uint8_t* aSrcBuffer,
                           const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToYUVImg(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::YUV422P, &RGBA32ToYUV422P);
}

UniquePtr<ImagePixelLayout>
Utils_YUV422P::ConvertFrom(Utils_BGRA32* aSrcUtils, const uint8_t* aSrcBuffer,
                           const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToYUVImg(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::YUV422P, &libyuv::ARGBToI422);
}

UniquePtr<ImagePixelLayout>
Utils_YUV422P::ConvertFrom(Utils_RGB24* aSrcUtils, const uint8_t* aSrcBuffer,
                           const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToYUVImg(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::YUV422P, &RGB24ToYUV422P);
}

UniquePtr<ImagePixelLayout>
Utils_YUV422P::ConvertFrom(Utils_BGR24* aSrcUtils, const uint8_t* aSrcBuffer,
                           const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToYUVImg(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::YUV422P, &BGR24ToYUV422P);
}

UniquePtr<ImagePixelLayout>
Utils_YUV422P::ConvertFrom(Utils_Gray8* aSrcFormat, const uint8_t* aSrcBuffer,
                           const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return nullptr;
}

// TODO: optimize me.
UniquePtr<ImagePixelLayout>
Utils_YUV422P::ConvertFrom(Utils_YUV444P* aSrcUtils, const uint8_t* aSrcBuffer,
                           const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return TwoPassConversion(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::YUV420P, this);
}

UniquePtr<ImagePixelLayout>
Utils_YUV422P::ConvertFrom(Utils_YUV422P* aSrcUtils, const uint8_t* aSrcBuffer,
                           const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return PureCopy(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::YUV422P);
}

UniquePtr<ImagePixelLayout>
Utils_YUV422P::ConvertFrom(Utils_YUV420P*, const uint8_t* aSrcBuffer,
                           const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  MOZ_ASSERT(aSrcBuffer, "Convert color from a null buffer.");
  MOZ_ASSERT(aSrcLayout, "Convert color from a null layout.");
  MOZ_ASSERT(aDstBuffer, "Convert color to a null buffer.");

  UniquePtr<ImagePixelLayout> layout =
    CreateDefaultLayout((*aSrcLayout)[0].mWidth,
                        (*aSrcLayout)[0].mHeight,
                        (*aSrcLayout)[0].mWidth);

  MOZ_ASSERT(layout, "Cannot create a ImagePixelLayout of YUV422P");

  const nsTArray<ChannelPixelLayout>& channels = *layout;

  const nsTArray<ChannelPixelLayout>& srcChannels = *aSrcLayout;

  libyuv::I420ToI422(aSrcBuffer + srcChannels[0].mOffset, srcChannels[0].mStride,
                     aSrcBuffer + srcChannels[1].mOffset, srcChannels[1].mStride,
                     aSrcBuffer + srcChannels[2].mOffset, srcChannels[2].mStride,
                     aDstBuffer + channels[0].mOffset, channels[0].mStride,
                     aDstBuffer + channels[1].mOffset, channels[1].mStride,
                     aDstBuffer + channels[2].mOffset, channels[2].mStride,
                     channels[0].mWidth, channels[0].mHeight);

  return layout;
}

// TODO: optimize me.
UniquePtr<ImagePixelLayout>
Utils_YUV422P::ConvertFrom(Utils_YUV420SP_NV12* aSrcUtils, const uint8_t* aSrcBuffer,
                           const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return TwoPassConversion(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::YUV420P, this);
}

// TODO: optimize me.
UniquePtr<ImagePixelLayout>
Utils_YUV422P::ConvertFrom(Utils_YUV420SP_NV21* aSrcUtils, const uint8_t* aSrcBuffer,
                           const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return TwoPassConversion(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::YUV420P, this);
}

UniquePtr<ImagePixelLayout>
Utils_YUV422P::ConvertFrom(Utils_HSV* aSrcUtils, const uint8_t* aSrcBuffer,
                           const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return TwoPassConversion(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::RGB24, this);
}

UniquePtr<ImagePixelLayout>
Utils_YUV422P::ConvertFrom(Utils_Lab* aSrcUtils, const uint8_t* aSrcBuffer,
                           const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return TwoPassConversion(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::RGB24, this);
}

UniquePtr<ImagePixelLayout>
Utils_YUV422P::ConvertFrom(Utils_Depth* aSrcUtils, const uint8_t* aSrcBuffer,
                           const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return nullptr;
}

bool
Utils_YUV422P::CanConvertFrom(ImageBitmapFormat aSrcFormat)
{
  if (aSrcFormat == ImageBitmapFormat::GRAY8 ||
      aSrcFormat == ImageBitmapFormat::DEPTH) {
    return false;
  }

  return true;
}

UniquePtr<ImagePixelLayout>
Utils_YUV422P::CreateDefaultLayout(uint32_t aWidth, uint32_t aHeight, uint32_t aStride)
{
  UniquePtr<ImagePixelLayout> layout(new ImagePixelLayout(mChannels));

  // set mChannels
  ChannelPixelLayout* ychannel = layout->AppendElement();
  ChannelPixelLayout* uchannel = layout->AppendElement();
  ChannelPixelLayout* vchannel = layout->AppendElement();
  ychannel->mOffset = 0;
  ychannel->mWidth  = aWidth;
  ychannel->mHeight = aHeight;
  ychannel->mDataType = ChannelPixelLayoutDataType::Uint8;
  ychannel->mStride = aStride;
  ychannel->mSkip   = 0; // aYSkip;

  uchannel->mOffset = ychannel->mOffset + ychannel->mStride * ychannel->mHeight;
  uchannel->mWidth  = (aWidth + 1) / 2;
  uchannel->mHeight = aHeight;
  uchannel->mDataType = ChannelPixelLayoutDataType::Uint8;
  uchannel->mStride = (aStride + 1) / 2;
  uchannel->mSkip   = 0; // aUSkip;

  vchannel->mOffset = uchannel->mOffset + uchannel->mStride * uchannel->mHeight;
  vchannel->mWidth  = (aWidth + 1) / 2;
  vchannel->mHeight = aHeight;
  vchannel->mDataType = ChannelPixelLayoutDataType::Uint8;
  vchannel->mStride = (aStride + 1) / 2;
  vchannel->mSkip   = 0; // aVSkip;

  return layout;
}

/*
 * Utils_YUV420P.
 */
/* static */Utils_YUV420P&
Utils_YUV420P::GetInstance()
{
  static Utils_YUV420P instance;
  return instance;
}

Utils_YUV420P::Utils_YUV420P()
: Utils(3, ChannelPixelLayoutDataType::Uint8)
{
}

uint32_t
Utils_YUV420P::NeededBufferSize(uint32_t aWidth, uint32_t aHeight)
{
  return aWidth * aHeight * mBytesPerPixelValue +
         2 * ((aWidth + 1) / 2) * ((aHeight + 1) / 2) * mBytesPerPixelValue;
}

UniquePtr<ImagePixelLayout>
Utils_YUV420P::ConvertTo(Utils* aDstFormat, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return aDstFormat->ConvertFrom(this, aSrcBuffer, aSrcLayout, aDstBuffer);
}

UniquePtr<ImagePixelLayout>
Utils_YUV420P::ConvertFrom(Utils_RGBA32* aSrcUtils, const uint8_t* aSrcBuffer,
                           const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToYUVImg(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::YUV420P, &libyuv::ABGRToI420);
}

UniquePtr<ImagePixelLayout>
Utils_YUV420P::ConvertFrom(Utils_BGRA32* aSrcUtils, const uint8_t* aSrcBuffer,
                           const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToYUVImg(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::YUV420P, &libyuv::ARGBToI420);
}

UniquePtr<ImagePixelLayout>
Utils_YUV420P::ConvertFrom(Utils_RGB24* aSrcUtils, const uint8_t* aSrcBuffer,
                           const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToYUVImg(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::YUV420P, &RGB24ToYUV420P);
}

UniquePtr<ImagePixelLayout>
Utils_YUV420P::ConvertFrom(Utils_BGR24* aSrcUtils, const uint8_t* aSrcBuffer,
                           const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToYUVImg(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::YUV420P, &BGR24ToYUV420P);
}

UniquePtr<ImagePixelLayout>
Utils_YUV420P::ConvertFrom(Utils_Gray8* aSrcFormat, const uint8_t* aSrcBuffer,
                           const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return nullptr;
}

UniquePtr<ImagePixelLayout>
Utils_YUV420P::ConvertFrom(Utils_YUV444P*, const uint8_t* aSrcBuffer,
                           const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  MOZ_ASSERT(aSrcBuffer, "Convert color from a null buffer.");
  MOZ_ASSERT(aSrcLayout, "Convert color from a null layout.");
  MOZ_ASSERT(aDstBuffer, "Convert color to a null buffer.");

  UniquePtr<ImagePixelLayout> layout =
    CreateDefaultLayout((*aSrcLayout)[0].mWidth,
                        (*aSrcLayout)[0].mHeight,
                        (*aSrcLayout)[0].mWidth);

  MOZ_ASSERT(layout, "Cannot create a ImagePixelLayout of YUV420P");

  const nsTArray<ChannelPixelLayout>& channels = *layout;

  const nsTArray<ChannelPixelLayout>& srcChannels = *aSrcLayout;

  libyuv::I444ToI420(aSrcBuffer + srcChannels[0].mOffset, srcChannels[0].mStride,
                     aSrcBuffer + srcChannels[1].mOffset, srcChannels[1].mStride,
                     aSrcBuffer + srcChannels[2].mOffset, srcChannels[2].mStride,
                     aDstBuffer + channels[0].mOffset, channels[0].mStride,
                     aDstBuffer + channels[1].mOffset, channels[1].mStride,
                     aDstBuffer + channels[2].mOffset, channels[2].mStride,
                     channels[0].mWidth, channels[0].mHeight);

  return layout;
}

UniquePtr<ImagePixelLayout>
Utils_YUV420P::ConvertFrom(Utils_YUV422P*, const uint8_t* aSrcBuffer,
                           const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  MOZ_ASSERT(aSrcBuffer, "Convert color from a null buffer.");
  MOZ_ASSERT(aSrcLayout, "Convert color from a null layout.");
  MOZ_ASSERT(aDstBuffer, "Convert color to a null buffer.");

  UniquePtr<ImagePixelLayout> layout =
    CreateDefaultLayout((*aSrcLayout)[0].mWidth,
                        (*aSrcLayout)[0].mHeight,
                        (*aSrcLayout)[0].mWidth);

  MOZ_ASSERT(layout, "Cannot create a ImagePixelLayout of YUV420P");

  const nsTArray<ChannelPixelLayout>& channels = *layout;

  const nsTArray<ChannelPixelLayout>& srcChannels = *aSrcLayout;

  libyuv::I422ToI420(aSrcBuffer + srcChannels[0].mOffset, srcChannels[0].mStride,
                     aSrcBuffer + srcChannels[1].mOffset, srcChannels[1].mStride,
                     aSrcBuffer + srcChannels[2].mOffset, srcChannels[2].mStride,
                     aDstBuffer + channels[0].mOffset, channels[0].mStride,
                     aDstBuffer + channels[1].mOffset, channels[1].mStride,
                     aDstBuffer + channels[2].mOffset, channels[2].mStride,
                     channels[0].mWidth, channels[0].mHeight);

  return layout;
}

UniquePtr<ImagePixelLayout>
Utils_YUV420P::ConvertFrom(Utils_YUV420P* aSrcUtils, const uint8_t* aSrcBuffer,
                           const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return PureCopy(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::YUV420P);
}

UniquePtr<ImagePixelLayout>
Utils_YUV420P::ConvertFrom(Utils_YUV420SP_NV12*, const uint8_t* aSrcBuffer,
                           const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  MOZ_ASSERT(aSrcBuffer, "Convert color from a null buffer.");
  MOZ_ASSERT(aSrcLayout, "Convert color from a null layout.");
  MOZ_ASSERT(aDstBuffer, "Convert color to a null buffer.");

  UniquePtr<ImagePixelLayout> layout =
    CreateDefaultLayout((*aSrcLayout)[0].mWidth,
                        (*aSrcLayout)[0].mHeight,
                        (*aSrcLayout)[0].mWidth);

  MOZ_ASSERT(layout, "Cannot create a ImagePixelLayout of YUV420P");

  const nsTArray<ChannelPixelLayout>& channels = *layout;

  const nsTArray<ChannelPixelLayout>& srcChannels = *aSrcLayout;

  libyuv::NV12ToI420(aSrcBuffer + srcChannels[0].mOffset, srcChannels[0].mStride,
                     aSrcBuffer + srcChannels[1].mOffset, srcChannels[1].mStride,
                     aDstBuffer + channels[0].mOffset, channels[0].mStride,
                     aDstBuffer + channels[1].mOffset, channels[1].mStride,
                     aDstBuffer + channels[2].mOffset, channels[2].mStride,
                     channels[0].mWidth, channels[0].mHeight);

  return layout;
}

UniquePtr<ImagePixelLayout>
Utils_YUV420P::ConvertFrom(Utils_YUV420SP_NV21*, const uint8_t* aSrcBuffer,
                           const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  MOZ_ASSERT(aSrcBuffer, "Convert color from a null buffer.");
  MOZ_ASSERT(aSrcLayout, "Convert color from a null layout.");
  MOZ_ASSERT(aDstBuffer, "Convert color to a null buffer.");

  UniquePtr<ImagePixelLayout> layout =
    CreateDefaultLayout((*aSrcLayout)[0].mWidth,
                        (*aSrcLayout)[0].mHeight,
                        (*aSrcLayout)[0].mWidth);

  MOZ_ASSERT(layout, "Cannot create a ImagePixelLayout of YUV420P");

  const nsTArray<ChannelPixelLayout>& channels = *layout;

  const nsTArray<ChannelPixelLayout>& srcChannels = *aSrcLayout;

  libyuv::NV21ToI420(aSrcBuffer + srcChannels[0].mOffset, srcChannels[0].mStride,
                     aSrcBuffer + srcChannels[1].mOffset, srcChannels[1].mStride,
                     aDstBuffer + channels[0].mOffset, channels[0].mStride,
                     aDstBuffer + channels[1].mOffset, channels[1].mStride,
                     aDstBuffer + channels[2].mOffset, channels[2].mStride,
                     channels[0].mWidth, channels[0].mHeight);

  return layout;
}

UniquePtr<ImagePixelLayout>
Utils_YUV420P::ConvertFrom(Utils_HSV* aSrcUtils, const uint8_t* aSrcBuffer,
                           const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return TwoPassConversion(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::RGB24, this);
}

UniquePtr<ImagePixelLayout>
Utils_YUV420P::ConvertFrom(Utils_Lab* aSrcUtils, const uint8_t* aSrcBuffer,
                           const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return TwoPassConversion(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::RGB24, this);
}

UniquePtr<ImagePixelLayout>
Utils_YUV420P::ConvertFrom(Utils_Depth* aSrcUtils, const uint8_t* aSrcBuffer,
                           const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return nullptr;
}

bool
Utils_YUV420P::CanConvertFrom(ImageBitmapFormat aSrcFormat)
{
  if (aSrcFormat == ImageBitmapFormat::GRAY8 ||
      aSrcFormat == ImageBitmapFormat::DEPTH) {
    return false;
  }

  return true;
}

UniquePtr<ImagePixelLayout>
Utils_YUV420P::CreateDefaultLayout(uint32_t aWidth, uint32_t aHeight, uint32_t aStride)
{
  UniquePtr<ImagePixelLayout> layout(new ImagePixelLayout(mChannels));

  // set mChannels
  ChannelPixelLayout* ychannel = layout->AppendElement();
  ChannelPixelLayout* uchannel = layout->AppendElement();
  ChannelPixelLayout* vchannel = layout->AppendElement();
  ychannel->mOffset = 0;
  ychannel->mWidth  = aWidth;
  ychannel->mHeight = aHeight;
  ychannel->mDataType = ChannelPixelLayoutDataType::Uint8;
  ychannel->mStride = aStride;
  ychannel->mSkip   = 0; // aYSkip;

  uchannel->mOffset = ychannel->mOffset + ychannel->mStride * ychannel->mHeight;
  uchannel->mWidth  = (aWidth + 1) / 2;
  uchannel->mHeight = (aHeight + 1) / 2;
  uchannel->mDataType = ChannelPixelLayoutDataType::Uint8;
  uchannel->mStride = (aStride + 1) / 2;
  uchannel->mSkip   = 0; // aUSkip;

  vchannel->mOffset = uchannel->mOffset + uchannel->mStride * uchannel->mHeight;
  vchannel->mWidth  = (aWidth + 1) / 2;
  vchannel->mHeight = (aHeight + 1) / 2;
  vchannel->mDataType = ChannelPixelLayoutDataType::Uint8;
  vchannel->mStride = (aStride + 1) / 2;
  vchannel->mSkip   = 0; // aVSkip;

  return layout;
}

/*
 * class Utils_YUV420SP_NV12.
 */
/* static */Utils_YUV420SP_NV12&
Utils_YUV420SP_NV12::GetInstance()
{
  static Utils_YUV420SP_NV12 instance;
  return instance;
}

Utils_YUV420SP_NV12::Utils_YUV420SP_NV12()
: Utils(3, ChannelPixelLayoutDataType::Uint8)
{
}

uint32_t
Utils_YUV420SP_NV12::NeededBufferSize(uint32_t aWidth, uint32_t aHeight)
{
  return aWidth * aHeight * mBytesPerPixelValue +
         2 * ((aWidth + 1) / 2) * ((aHeight + 1) / 2) * mBytesPerPixelValue;
}

UniquePtr<ImagePixelLayout>
Utils_YUV420SP_NV12::ConvertTo(Utils* aDstFormat, const uint8_t* aSrcBuffer,
                               const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return aDstFormat->ConvertFrom(this, aSrcBuffer, aSrcLayout, aDstBuffer);
}

UniquePtr<ImagePixelLayout>
Utils_YUV420SP_NV12::ConvertFrom(Utils_RGBA32* aSrcUtils, const uint8_t* aSrcBuffer,
                                 const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToNVImg(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::YUV420SP_NV12, &RGBA32ToNV12);
}

UniquePtr<ImagePixelLayout>
Utils_YUV420SP_NV12::ConvertFrom(Utils_BGRA32* aSrcUtils, const uint8_t* aSrcBuffer,
                                 const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToNVImg(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::YUV420SP_NV12, &libyuv::ARGBToNV12);
}

UniquePtr<ImagePixelLayout>
Utils_YUV420SP_NV12::ConvertFrom(Utils_RGB24* aSrcUtils, const uint8_t* aSrcBuffer,
                                 const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToNVImg(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::YUV420SP_NV12, &RGB24ToNV12);
}

UniquePtr<ImagePixelLayout>
Utils_YUV420SP_NV12::ConvertFrom(Utils_BGR24* aSrcUtils, const uint8_t* aSrcBuffer,
                                 const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToNVImg(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::YUV420SP_NV12, &BGR24ToNV12);
}

UniquePtr<ImagePixelLayout>
Utils_YUV420SP_NV12::ConvertFrom(Utils_Gray8* aSrcFormat, const uint8_t* aSrcBuffer,
                                 const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return nullptr;
}

// TODO: optimize me.
UniquePtr<ImagePixelLayout>
Utils_YUV420SP_NV12::ConvertFrom(Utils_YUV444P* aSrcUtils, const uint8_t* aSrcBuffer,
                                 const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return TwoPassConversion(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::YUV420P, this);
}

// TODO: optimize me.
UniquePtr<ImagePixelLayout>
Utils_YUV420SP_NV12::ConvertFrom(Utils_YUV422P* aSrcUtils, const uint8_t* aSrcBuffer,
                                 const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return TwoPassConversion(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::YUV420P, this);
}

UniquePtr<ImagePixelLayout>
Utils_YUV420SP_NV12::ConvertFrom(Utils_YUV420P*, const uint8_t* aSrcBuffer,
                                 const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  MOZ_ASSERT(aSrcBuffer, "Convert color from a null buffer.");
  MOZ_ASSERT(aSrcLayout, "Convert color from a null layout.");
  MOZ_ASSERT(aDstBuffer, "Convert color to a null buffer.");

  UniquePtr<ImagePixelLayout> layout =
    CreateDefaultLayout((*aSrcLayout)[0].mWidth,
                        (*aSrcLayout)[0].mHeight,
                        (*aSrcLayout)[0].mWidth);

  MOZ_ASSERT(layout, "Cannot create a ImagePixelLayout of YUV420SP_NV12");

  const nsTArray<ChannelPixelLayout>& channels = *layout;

  const nsTArray<ChannelPixelLayout>& srcChannels = *aSrcLayout;

  libyuv::I420ToNV12(aSrcBuffer + srcChannels[0].mOffset, srcChannels[0].mStride,
                     aSrcBuffer + srcChannels[1].mOffset, srcChannels[1].mStride,
                     aSrcBuffer + srcChannels[2].mOffset, srcChannels[2].mStride,
                     aDstBuffer + channels[0].mOffset, channels[0].mStride,
                     aDstBuffer + channels[1].mOffset, channels[1].mStride,
                     channels[0].mWidth, channels[0].mHeight);

  return layout;
}

UniquePtr<ImagePixelLayout>
Utils_YUV420SP_NV12::ConvertFrom(Utils_YUV420SP_NV12* aSrcUtils, const uint8_t* aSrcBuffer,
                                 const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return PureCopy(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::YUV420SP_NV12);
}

// TODO: optimize me.
UniquePtr<ImagePixelLayout>
Utils_YUV420SP_NV12::ConvertFrom(Utils_YUV420SP_NV21* aSrcUtils, const uint8_t* aSrcBuffer,
                                 const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return TwoPassConversion(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::YUV420P, this);
}

UniquePtr<ImagePixelLayout>
Utils_YUV420SP_NV12::ConvertFrom(Utils_HSV* aSrcUtils, const uint8_t* aSrcBuffer,
                                 const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return TwoPassConversion(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::RGB24, this);
}

UniquePtr<ImagePixelLayout>
Utils_YUV420SP_NV12::ConvertFrom(Utils_Lab* aSrcUtils, const uint8_t* aSrcBuffer,
                                 const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return TwoPassConversion(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::RGB24, this);
}

UniquePtr<ImagePixelLayout>
Utils_YUV420SP_NV12::ConvertFrom(Utils_Depth* aSrcUtils, const uint8_t* aSrcBuffer,
                                 const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return nullptr;
}

bool
Utils_YUV420SP_NV12::CanConvertFrom(ImageBitmapFormat aSrcFormat)
{
  if (aSrcFormat == ImageBitmapFormat::GRAY8 ||
      aSrcFormat == ImageBitmapFormat::DEPTH) {
    return false;
  }

  return true;
}

UniquePtr<ImagePixelLayout>
Utils_YUV420SP_NV12::CreateDefaultLayout(uint32_t aWidth, uint32_t aHeight, uint32_t aStride)
{
  UniquePtr<ImagePixelLayout> layout(new ImagePixelLayout(mChannels));

  // set mChannels
  ChannelPixelLayout* ychannel = layout->AppendElement();
  ChannelPixelLayout* uchannel = layout->AppendElement();
  ChannelPixelLayout* vchannel = layout->AppendElement();
  ychannel->mOffset = 0;
  ychannel->mWidth  = aWidth;
  ychannel->mHeight = aHeight;
  ychannel->mDataType = ChannelPixelLayoutDataType::Uint8;
  ychannel->mStride = aStride;
  ychannel->mSkip   = 0; // aYSkip;

  uchannel->mOffset = ychannel->mOffset + ychannel->mStride * ychannel->mHeight;
  uchannel->mWidth  = (aWidth + 1) / 2;
  uchannel->mHeight = (aHeight + 1) / 2;
  uchannel->mDataType = ChannelPixelLayoutDataType::Uint8;
  uchannel->mStride = uchannel->mWidth * 2;
  uchannel->mSkip   = 1; // aUSkip;

  vchannel->mOffset = ychannel->mOffset + ychannel->mStride * ychannel->mHeight + 1;
  vchannel->mWidth  = (aWidth + 1) / 2;
  vchannel->mHeight = (aHeight + 1) / 2;
  vchannel->mDataType = ChannelPixelLayoutDataType::Uint8;
  vchannel->mStride = vchannel->mWidth * 2;
  vchannel->mSkip   = 1; // aVSkip;

  return layout;
}

/*
 * class Utils_YUV420SP_NV21.
 */
/* static */Utils_YUV420SP_NV21&
Utils_YUV420SP_NV21::GetInstance()
{
  static Utils_YUV420SP_NV21 instance;
  return instance;
}

Utils_YUV420SP_NV21::Utils_YUV420SP_NV21()
: Utils(3, ChannelPixelLayoutDataType::Uint8)
{
}

uint32_t
Utils_YUV420SP_NV21::NeededBufferSize(uint32_t aWidth, uint32_t aHeight)
{
  return aWidth * aHeight * mBytesPerPixelValue +
         2 * ((aWidth + 1) / 2) * ((aHeight + 1) / 2) * mBytesPerPixelValue;
}

UniquePtr<ImagePixelLayout>
Utils_YUV420SP_NV21::ConvertTo(Utils* aDstFormat, const uint8_t* aSrcBuffer,
                               const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return aDstFormat->ConvertFrom(this, aSrcBuffer, aSrcLayout, aDstBuffer);
}

UniquePtr<ImagePixelLayout>
Utils_YUV420SP_NV21::ConvertFrom(Utils_RGBA32* aSrcUtils, const uint8_t* aSrcBuffer,
                                 const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToNVImg(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::YUV420SP_NV21, &RGBA32ToNV21);
}

UniquePtr<ImagePixelLayout>
Utils_YUV420SP_NV21::ConvertFrom(Utils_BGRA32* aSrcUtils, const uint8_t* aSrcBuffer,
                                 const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToNVImg(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::YUV420SP_NV21, &libyuv::ARGBToNV21);
}

UniquePtr<ImagePixelLayout>
Utils_YUV420SP_NV21::ConvertFrom(Utils_RGB24* aSrcUtils, const uint8_t* aSrcBuffer,
                                 const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToNVImg(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::YUV420SP_NV21, &RGB24ToNV21);
}

UniquePtr<ImagePixelLayout>
Utils_YUV420SP_NV21::ConvertFrom(Utils_BGR24* aSrcUtils, const uint8_t* aSrcBuffer,
                                 const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToNVImg(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::YUV420SP_NV21, &BGR24ToNV21);
}

UniquePtr<ImagePixelLayout>
Utils_YUV420SP_NV21::ConvertFrom(Utils_Gray8* aSrcFormat, const uint8_t* aSrcBuffer,
                                 const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return nullptr;
}

// TODO: optimize me.
UniquePtr<ImagePixelLayout>
Utils_YUV420SP_NV21::ConvertFrom(Utils_YUV444P* aSrcUtils, const uint8_t* aSrcBuffer,
                                 const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return TwoPassConversion(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::YUV420P, this);
}

// TODO: optimize me.
UniquePtr<ImagePixelLayout>
Utils_YUV420SP_NV21::ConvertFrom(Utils_YUV422P* aSrcUtils, const uint8_t* aSrcBuffer,
                                 const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return TwoPassConversion(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::YUV420P, this);
}

UniquePtr<ImagePixelLayout>
Utils_YUV420SP_NV21::ConvertFrom(Utils_YUV420P*, const uint8_t* aSrcBuffer,
                                 const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  MOZ_ASSERT(aSrcBuffer, "Convert color from a null buffer.");
  MOZ_ASSERT(aSrcLayout, "Convert color from a null layout.");
  MOZ_ASSERT(aDstBuffer, "Convert color to a null buffer.");

  UniquePtr<ImagePixelLayout> layout =
    CreateDefaultLayout((*aSrcLayout)[0].mWidth,
                        (*aSrcLayout)[0].mHeight,
                        (*aSrcLayout)[0].mWidth);

  MOZ_ASSERT(layout, "Cannot create a ImagePixelLayout of YUV420SP_NV21");

  const nsTArray<ChannelPixelLayout>& channels = *layout;

  const nsTArray<ChannelPixelLayout>& srcChannels = *aSrcLayout;

  libyuv::I420ToNV21(aSrcBuffer + srcChannels[0].mOffset, srcChannels[0].mStride,
                     aSrcBuffer + srcChannels[1].mOffset, srcChannels[1].mStride,
                     aSrcBuffer + srcChannels[2].mOffset, srcChannels[2].mStride,
                     aDstBuffer + channels[0].mOffset, channels[0].mStride,
                     aDstBuffer + channels[1].mOffset, channels[1].mStride,
                     channels[0].mWidth, channels[0].mHeight);

  return layout;
}

// TODO: optimize me.
UniquePtr<ImagePixelLayout>
Utils_YUV420SP_NV21::ConvertFrom(Utils_YUV420SP_NV12* aSrcUtils, const uint8_t* aSrcBuffer,
                                 const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return TwoPassConversion(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::YUV420P, this);
}

UniquePtr<ImagePixelLayout>
Utils_YUV420SP_NV21::ConvertFrom(Utils_YUV420SP_NV21* aSrcUtils, const uint8_t* aSrcBuffer,
                                 const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return PureCopy(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::YUV420SP_NV21);
}

UniquePtr<ImagePixelLayout>
Utils_YUV420SP_NV21::ConvertFrom(Utils_HSV* aSrcUtils, const uint8_t* aSrcBuffer,
                                 const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return TwoPassConversion(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::RGB24, this);
}

UniquePtr<ImagePixelLayout>
Utils_YUV420SP_NV21::ConvertFrom(Utils_Lab* aSrcUtils, const uint8_t* aSrcBuffer,
                                 const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return TwoPassConversion(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::RGB24, this);
}

UniquePtr<ImagePixelLayout>
Utils_YUV420SP_NV21::ConvertFrom(Utils_Depth* aSrcUtils, const uint8_t* aSrcBuffer,
                                 const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return nullptr;
}

bool
Utils_YUV420SP_NV21::CanConvertFrom(ImageBitmapFormat aSrcFormat)
{
  if (aSrcFormat == ImageBitmapFormat::GRAY8 ||
      aSrcFormat == ImageBitmapFormat::DEPTH) {
    return false;
  }

  return true;
}

UniquePtr<ImagePixelLayout>
Utils_YUV420SP_NV21::CreateDefaultLayout(uint32_t aWidth, uint32_t aHeight, uint32_t aStride)
{
  UniquePtr<ImagePixelLayout> layout(new ImagePixelLayout(mChannels));

  // set mChannels
  ChannelPixelLayout* ychannel = layout->AppendElement();
  ChannelPixelLayout* vchannel = layout->AppendElement(); // v is the 2nd channel.
  ChannelPixelLayout* uchannel = layout->AppendElement(); // u is the 3rd channel.
  ychannel->mOffset = 0;
  ychannel->mWidth  = aWidth;
  ychannel->mHeight = aHeight;
  ychannel->mDataType = ChannelPixelLayoutDataType::Uint8;
  ychannel->mStride = aStride;
  ychannel->mSkip   = 0; // aYSkip;

  vchannel->mOffset = ychannel->mOffset + ychannel->mStride * ychannel->mHeight;
  vchannel->mWidth  = (aWidth + 1) / 2;
  vchannel->mHeight = (aHeight + 1) / 2;
  vchannel->mDataType = ChannelPixelLayoutDataType::Uint8;
  vchannel->mStride = vchannel->mWidth * 2;
  vchannel->mSkip   = 1; // aVSkip;

  uchannel->mOffset = ychannel->mOffset + ychannel->mStride * ychannel->mHeight + 1;
  uchannel->mWidth  = (aWidth + 1) / 2;
  uchannel->mHeight = (aHeight + 1) / 2;
  uchannel->mDataType = ChannelPixelLayoutDataType::Uint8;
  uchannel->mStride = uchannel->mWidth * 2;
  uchannel->mSkip   = 1; // aUSkip;

  return layout;
}

/*
 * Utils_HSV.
 */
/* static */Utils_HSV&
Utils_HSV::GetInstance()
{
  static Utils_HSV instance;
  return instance;
}

Utils_HSV::Utils_HSV()
: Utils(3, ChannelPixelLayoutDataType::Float32)
{
}

uint32_t
Utils_HSV::NeededBufferSize(uint32_t aWidth, uint32_t aHeight)
{
  return aWidth * aHeight * (uint32_t)mChannels * mBytesPerPixelValue;
}

UniquePtr<ImagePixelLayout>
Utils_HSV::ConvertTo(Utils* aDstFormat, const uint8_t* aSrcBuffer,
                     const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return aDstFormat->ConvertFrom(this, aSrcBuffer, aSrcLayout, aDstBuffer);
}

UniquePtr<ImagePixelLayout>
Utils_HSV::ConvertFrom(Utils_RGBA32* aSrcFormat, const uint8_t* aSrcBuffer,
                       const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToSimpleImg<uint8_t, float>(aSrcFormat, aSrcBuffer, aSrcLayout, (float*)aDstBuffer, ImageBitmapFormat::HSV, 3, &RGBA32ToHSV);
}

UniquePtr<ImagePixelLayout>
Utils_HSV::ConvertFrom(Utils_BGRA32* aSrcFormat, const uint8_t* aSrcBuffer,
                       const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToSimpleImg<uint8_t, float>(aSrcFormat, aSrcBuffer, aSrcLayout, (float*)aDstBuffer, ImageBitmapFormat::HSV, 3, &BGRA32ToHSV);
}

UniquePtr<ImagePixelLayout>
Utils_HSV::ConvertFrom(Utils_RGB24* aSrcUtils, const uint8_t* aSrcBuffer,
                       const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToSimpleImg<uint8_t, float>(aSrcUtils, aSrcBuffer, aSrcLayout, (float*)aDstBuffer, ImageBitmapFormat::HSV, 3, &RGB24ToHSV);
}

UniquePtr<ImagePixelLayout>
Utils_HSV::ConvertFrom(Utils_BGR24* aSrcUtils, const uint8_t* aSrcBuffer,
                       const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToSimpleImg<uint8_t, float>(aSrcUtils, aSrcBuffer, aSrcLayout, (float*)aDstBuffer, ImageBitmapFormat::HSV, 3, &BGR24ToHSV);
}

UniquePtr<ImagePixelLayout>
Utils_HSV::ConvertFrom(Utils_Gray8* aSrcFormat, const uint8_t* aSrcBuffer,
                       const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return nullptr;
}

UniquePtr<ImagePixelLayout>
Utils_HSV::ConvertFrom(Utils_YUV444P* aSrcUtils, const uint8_t* aSrcBuffer,
                       const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return TwoPassConversion(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::RGB24, this);
}

UniquePtr<ImagePixelLayout>
Utils_HSV::ConvertFrom(Utils_YUV422P* aSrcUtils, const uint8_t* aSrcBuffer,
                       const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return TwoPassConversion(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::RGB24, this);
}

UniquePtr<ImagePixelLayout>
Utils_HSV::ConvertFrom(Utils_YUV420P* aSrcUtils, const uint8_t* aSrcBuffer,
                       const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return TwoPassConversion(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::RGB24, this);
}

UniquePtr<ImagePixelLayout>
Utils_HSV::ConvertFrom(Utils_YUV420SP_NV12* aSrcUtils, const uint8_t* aSrcBuffer,
                       const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return TwoPassConversion(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::RGB24, this);
}

UniquePtr<ImagePixelLayout>
Utils_HSV::ConvertFrom(Utils_YUV420SP_NV21* aSrcUtils, const uint8_t* aSrcBuffer,
                       const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return TwoPassConversion(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::RGB24, this);
}

UniquePtr<ImagePixelLayout>
Utils_HSV::ConvertFrom(Utils_HSV* aSrcUtils, const uint8_t* aSrcBuffer,
                       const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return PureCopy(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::HSV);
}

UniquePtr<ImagePixelLayout>
Utils_HSV::ConvertFrom(Utils_Lab* aSrcUtils, const uint8_t* aSrcBuffer,
                       const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return TwoPassConversion(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::RGB24, this);
}

UniquePtr<ImagePixelLayout>
Utils_HSV::ConvertFrom(Utils_Depth* aSrcUtils, const uint8_t* aSrcBuffer,
                       const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return nullptr;
}

bool
Utils_HSV::CanConvertFrom(ImageBitmapFormat aSrcFormat)
{
  if (aSrcFormat == ImageBitmapFormat::GRAY8 ||
      aSrcFormat == ImageBitmapFormat::DEPTH) {
    return false;
  }

  return true;
}

UniquePtr<ImagePixelLayout>
Utils_HSV::CreateDefaultLayout(uint32_t aWidth, uint32_t aHeight, uint32_t aStride)
{
  return CreateDefaultLayoutForSimpleImage(aWidth, aHeight, aStride, mChannels, mBytesPerPixelValue, mDataType);
}

/*
 * Utils_Lab.
 */
/* static */Utils_Lab&
Utils_Lab::GetInstance()
{
  static Utils_Lab instance;
  return instance;
}

Utils_Lab::Utils_Lab()
: Utils(3, ChannelPixelLayoutDataType::Float32)
{
}

uint32_t
Utils_Lab::NeededBufferSize(uint32_t aWidth, uint32_t aHeight)
{
  return aWidth * aHeight * (uint32_t)mChannels * mBytesPerPixelValue;
}

UniquePtr<ImagePixelLayout>
Utils_Lab::ConvertTo(Utils* aDstFormat, const uint8_t* aSrcBuffer,
                     const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return aDstFormat->ConvertFrom(this, aSrcBuffer, aSrcLayout, aDstBuffer);
}

UniquePtr<ImagePixelLayout>
Utils_Lab::ConvertFrom(Utils_RGBA32* aSrcUtils, const uint8_t* aSrcBuffer,
                       const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToSimpleImg<uint8_t, float>(aSrcUtils, aSrcBuffer, aSrcLayout, (float*)aDstBuffer, ImageBitmapFormat::Lab, 3, &RGBA32ToLab);
}

UniquePtr<ImagePixelLayout>
Utils_Lab::ConvertFrom(Utils_BGRA32* aSrcUtils, const uint8_t* aSrcBuffer,
                       const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToSimpleImg<uint8_t, float>(aSrcUtils, aSrcBuffer, aSrcLayout, (float*)aDstBuffer, ImageBitmapFormat::Lab, 3, &BGRA32ToLab);
}

UniquePtr<ImagePixelLayout>
Utils_Lab::ConvertFrom(Utils_RGB24* aSrcUtils, const uint8_t* aSrcBuffer,
                       const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToSimpleImg<uint8_t, float>(aSrcUtils, aSrcBuffer, aSrcLayout, (float*)aDstBuffer, ImageBitmapFormat::Lab, 3, &RGB24ToLab);
}

UniquePtr<ImagePixelLayout>
Utils_Lab::ConvertFrom(Utils_BGR24* aSrcUtils, const uint8_t* aSrcBuffer,
                       const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return CvtSimpleImgToSimpleImg<uint8_t, float>(aSrcUtils, aSrcBuffer, aSrcLayout, (float*)aDstBuffer, ImageBitmapFormat::Lab, 3, &BGR24ToLab);
}

UniquePtr<ImagePixelLayout>
Utils_Lab::ConvertFrom(Utils_Gray8* aSrcFormat, const uint8_t* aSrcBuffer,
                       const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return nullptr;
}

UniquePtr<ImagePixelLayout>
Utils_Lab::ConvertFrom(Utils_YUV444P* aSrcUtils, const uint8_t* aSrcBuffer,
                       const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return TwoPassConversion(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::RGB24, this);
}

UniquePtr<ImagePixelLayout>
Utils_Lab::ConvertFrom(Utils_YUV422P* aSrcUtils, const uint8_t* aSrcBuffer,
                       const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return TwoPassConversion(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::RGB24, this);
}

UniquePtr<ImagePixelLayout>
Utils_Lab::ConvertFrom(Utils_YUV420P* aSrcUtils, const uint8_t* aSrcBuffer,
                       const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return TwoPassConversion(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::RGB24, this);
}

UniquePtr<ImagePixelLayout>
Utils_Lab::ConvertFrom(Utils_YUV420SP_NV12* aSrcUtils, const uint8_t* aSrcBuffer,
                       const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return TwoPassConversion(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::RGB24, this);
}

UniquePtr<ImagePixelLayout>
Utils_Lab::ConvertFrom(Utils_YUV420SP_NV21* aSrcUtils, const uint8_t* aSrcBuffer,
                       const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return TwoPassConversion(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::RGB24, this);
}

UniquePtr<ImagePixelLayout>
Utils_Lab::ConvertFrom(Utils_HSV* aSrcUtils, const uint8_t* aSrcBuffer,
                       const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return TwoPassConversion(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::RGB24, this);
}

UniquePtr<ImagePixelLayout>
Utils_Lab::ConvertFrom(Utils_Lab* aSrcUtils, const uint8_t* aSrcBuffer,
                       const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return PureCopy(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::Lab);
}

UniquePtr<ImagePixelLayout>
Utils_Lab::ConvertFrom(Utils_Depth* aSrcUtils, const uint8_t* aSrcBuffer,
                       const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return nullptr;
}

bool
Utils_Lab::CanConvertFrom(ImageBitmapFormat aSrcFormat)
{
  if (aSrcFormat == ImageBitmapFormat::GRAY8 ||
      aSrcFormat == ImageBitmapFormat::DEPTH) {
    return false;
  }

  return true;
}

UniquePtr<ImagePixelLayout>
Utils_Lab::CreateDefaultLayout(uint32_t aWidth, uint32_t aHeight, uint32_t aStride)
{
  return CreateDefaultLayoutForSimpleImage(aWidth, aHeight, aStride, mChannels, mBytesPerPixelValue, mDataType);
}

/*
 * Utils_Depth.
 */
/* static */Utils_Depth&
Utils_Depth::GetInstance()
{
  static Utils_Depth instance;
  return instance;
}

Utils_Depth::Utils_Depth()
: Utils(1, ChannelPixelLayoutDataType::Uint16)
{
}

uint32_t
Utils_Depth::NeededBufferSize(uint32_t aWidth, uint32_t aHeight)
{
  return aWidth * aHeight * (uint32_t)mChannels * mBytesPerPixelValue;
}

UniquePtr<ImagePixelLayout>
Utils_Depth::ConvertTo(Utils* aDstFormat, const uint8_t* aSrcBuffer,
                       const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return aDstFormat->ConvertFrom(this, aSrcBuffer, aSrcLayout, aDstBuffer);
}

UniquePtr<ImagePixelLayout>
Utils_Depth::ConvertFrom(Utils_RGBA32* aSrcFormat, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return nullptr;
}

UniquePtr<ImagePixelLayout>
Utils_Depth::ConvertFrom(Utils_BGRA32* aSrcFormat, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return nullptr;
}

UniquePtr<ImagePixelLayout>
Utils_Depth::ConvertFrom(Utils_RGB24* aSrcUtils, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return nullptr;
}

UniquePtr<ImagePixelLayout>
Utils_Depth::ConvertFrom(Utils_BGR24* aSrcUtils, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return nullptr;
}

UniquePtr<ImagePixelLayout>
Utils_Depth::ConvertFrom(Utils_Gray8* aSrcFormat, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return nullptr;
}

UniquePtr<ImagePixelLayout>
Utils_Depth::ConvertFrom(Utils_YUV444P* aSrcUtils, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return nullptr;
}

UniquePtr<ImagePixelLayout>
Utils_Depth::ConvertFrom(Utils_YUV422P* aSrcUtils, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return nullptr;
}

UniquePtr<ImagePixelLayout>
Utils_Depth::ConvertFrom(Utils_YUV420P* aSrcUtils, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return nullptr;
}

UniquePtr<ImagePixelLayout>
Utils_Depth::ConvertFrom(Utils_YUV420SP_NV12* aSrcUtils, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return nullptr;
}

UniquePtr<ImagePixelLayout>
Utils_Depth::ConvertFrom(Utils_YUV420SP_NV21* aSrcUtils, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return nullptr;
}

UniquePtr<ImagePixelLayout>
Utils_Depth::ConvertFrom(Utils_HSV* aSrcUtils, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return nullptr;
}

UniquePtr<ImagePixelLayout>
Utils_Depth::ConvertFrom(Utils_Lab* aSrcUtils, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return nullptr;
}

UniquePtr<ImagePixelLayout>
Utils_Depth::ConvertFrom(Utils_Depth* aSrcUtils, const uint8_t* aSrcBuffer,
                         const ImagePixelLayout* aSrcLayout, uint8_t* aDstBuffer)
{
  return PureCopy(aSrcUtils, aSrcBuffer, aSrcLayout, aDstBuffer, ImageBitmapFormat::DEPTH);
}

bool
Utils_Depth::CanConvertFrom(ImageBitmapFormat aSrcFormat)
{
  if (aSrcFormat == ImageBitmapFormat::DEPTH ) {
    return true;
  }

  return false;
}

UniquePtr<ImagePixelLayout>
Utils_Depth::CreateDefaultLayout(uint32_t aWidth, uint32_t aHeight, uint32_t aStride)
{
  return CreateDefaultLayoutForSimpleImage(aWidth, aHeight, aStride, mChannels, mBytesPerPixelValue, mDataType);
}

} // namespace imagebitmapformat

/*
 * Global functions.
 */

using namespace mozilla::dom::imagebitmapformat;

UniquePtr<ImagePixelLayout>
CreateDefaultPixelLayout(ImageBitmapFormat aFormat, uint32_t aWidth,
                         uint32_t aHeight, uint32_t aStride)
{
  UtilsUniquePtr format = Utils::GetUtils(aFormat);
  MOZ_ASSERT(format, "Cannot get a valid ImageBitmapFormatUtils instance.");

  return format->CreateDefaultLayout(aWidth, aHeight, aStride);
}

UniquePtr<ImagePixelLayout>
CreatePixelLayoutFromPlanarYCbCrData(const layers::PlanarYCbCrData* aData)
{
  if (!aData) {
    // something wrong
    return nullptr;
  }

  UniquePtr<ImagePixelLayout> layout(new ImagePixelLayout(3));

  ChannelPixelLayout* ychannel = layout->AppendElement();
  ChannelPixelLayout* uchannel = layout->AppendElement();
  ChannelPixelLayout* vchannel = layout->AppendElement();

  ychannel->mOffset = 0;

  if (aData->mCrChannel - aData->mCbChannel > 0) {
    uchannel->mOffset = ychannel->mOffset + (aData->mCbChannel - aData->mYChannel);
    vchannel->mOffset = uchannel->mOffset + (aData->mCrChannel - aData->mCbChannel);
  } else {
    uchannel->mOffset = ychannel->mOffset + (aData->mCrChannel - aData->mYChannel);
    vchannel->mOffset = uchannel->mOffset + (aData->mCbChannel - aData->mCrChannel);
  }

  ychannel->mWidth = aData->mYSize.width;
  ychannel->mHeight = aData->mYSize.height;
  ychannel->mDataType = ChannelPixelLayoutDataType::Uint8;
  ychannel->mStride = aData->mYStride;
  ychannel->mSkip = aData->mYSkip;

  uchannel->mWidth = aData->mCbCrSize.width;
  uchannel->mHeight = aData->mCbCrSize.height;
  uchannel->mDataType = ChannelPixelLayoutDataType::Uint8;
  uchannel->mStride = aData->mCbCrStride;
  uchannel->mSkip = aData->mCbSkip;

  vchannel->mWidth = aData->mCbCrSize.width;
  vchannel->mHeight = aData->mCbCrSize.height;
  vchannel->mDataType = ChannelPixelLayoutDataType::Uint8;
  vchannel->mStride = aData->mCbCrStride;
  vchannel->mSkip = aData->mCrSkip;

  return layout;
}

uint8_t
GetChannelCountOfImageFormat(ImageBitmapFormat aFormat)
{
  UtilsUniquePtr format = Utils::GetUtils(aFormat);
  MOZ_ASSERT(format, "Cannot get a valid ImageBitmapFormatUtils instance.");

  return format->GetChannelCount();
}

uint32_t
CalculateImageBufferSize(ImageBitmapFormat aFormat,
                         uint32_t aWidth, uint32_t aHeight)
{
  UtilsUniquePtr format = Utils::GetUtils(aFormat);
  MOZ_ASSERT(format, "Cannot get a valid ImageBitmapFormatUtils instance.");

  return format->NeededBufferSize(aWidth, aHeight);
}

UniquePtr<ImagePixelLayout>
CopyAndConvertImageData(ImageBitmapFormat aSrcFormat,
                        const uint8_t* aSrcBuffer,
                        const ImagePixelLayout* aSrcLayout,
                        ImageBitmapFormat aDstFormat,
                        uint8_t* aDstBuffer)
{
  MOZ_ASSERT(aSrcBuffer, "Convert color from a null buffer.");
  MOZ_ASSERT(aSrcLayout, "Convert color from a null layout.");
  MOZ_ASSERT(aDstBuffer, "Convert color to a null buffer.");

  UtilsUniquePtr srcFormat = Utils::GetUtils(aSrcFormat);
  UtilsUniquePtr dstFormat = Utils::GetUtils(aDstFormat);
  MOZ_ASSERT(srcFormat, "Cannot get a valid ImageBitmapFormatUtils instance.");
  MOZ_ASSERT(dstFormat, "Cannot get a valid ImageBitmapFormatUtils instance.");

  return srcFormat->ConvertTo(dstFormat.get(), aSrcBuffer, aSrcLayout, aDstBuffer);
}

ImageBitmapFormat
FindBestMatchingFromat(ImageBitmapFormat aSrcFormat,
                       const Sequence<ImageBitmapFormat>& aCandidates) {

  for(auto& candidate : aCandidates) {
    UtilsUniquePtr candidateFormat = Utils::GetUtils(candidate);
    MOZ_ASSERT(candidateFormat, "Cannot get a valid ImageBitmapFormatUtils instance.");

    if (candidateFormat->CanConvertFrom(aSrcFormat)) {
      return candidate;
    }
  }

  return ImageBitmapFormat::EndGuard_;
}

} // namespace dom
} // namespace mozilla
