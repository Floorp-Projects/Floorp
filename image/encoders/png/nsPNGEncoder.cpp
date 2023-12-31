/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageLogging.h"
#include "nsCRT.h"
#include "nsPNGEncoder.h"
#include "nsStreamUtils.h"
#include "nsString.h"
#include "prprf.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/UniquePtrExtensions.h"

using namespace mozilla;

static LazyLogModule sPNGEncoderLog("PNGEncoder");

NS_IMPL_ISUPPORTS(nsPNGEncoder, imgIEncoder, nsIInputStream,
                  nsIAsyncInputStream)

#define DEFAULT_ZLIB_LEVEL 3
#define DEFAULT_FILTERS PNG_FILTER_SUB

nsPNGEncoder::nsPNGEncoder()
    : mPNG(nullptr),
      mPNGinfo(nullptr),
      mIsAnimation(false),
      mFinished(false),
      mImageBuffer(nullptr),
      mImageBufferSize(0),
      mImageBufferUsed(0),
      mImageBufferReadPoint(0),
      mCallback(nullptr),
      mCallbackTarget(nullptr),
      mNotifyThreshold(0),
      mReentrantMonitor("nsPNGEncoder.mReentrantMonitor") {}

nsPNGEncoder::~nsPNGEncoder() {
  if (mImageBuffer) {
    free(mImageBuffer);
    mImageBuffer = nullptr;
  }
  // don't leak if EndImageEncode wasn't called
  if (mPNG) {
    png_destroy_write_struct(&mPNG, &mPNGinfo);
  }
}

// nsPNGEncoder::InitFromData
//
//    One output option is supported: "transparency=none" means that the
//    output PNG will not have an alpha channel, even if the input does.
//
//    Based partially on gfx/cairo/cairo/src/cairo-png.c
//    See also media/libpng/libpng-manual.txt

NS_IMETHODIMP
nsPNGEncoder::InitFromData(const uint8_t* aData,
                           uint32_t aLength,  // (unused, req'd by JS)
                           uint32_t aWidth, uint32_t aHeight, uint32_t aStride,
                           uint32_t aInputFormat,
                           const nsAString& aOutputOptions) {
  NS_ENSURE_ARG(aData);
  nsresult rv;

  rv = StartImageEncode(aWidth, aHeight, aInputFormat, aOutputOptions);
  if (!NS_SUCCEEDED(rv)) {
    return rv;
  }

  rv = AddImageFrame(aData, aLength, aWidth, aHeight, aStride, aInputFormat,
                     aOutputOptions);
  if (!NS_SUCCEEDED(rv)) {
    return rv;
  }

  rv = EndImageEncode();

  return rv;
}

// nsPNGEncoder::StartImageEncode
//
//
// See ::InitFromData for other info.
NS_IMETHODIMP
nsPNGEncoder::StartImageEncode(uint32_t aWidth, uint32_t aHeight,
                               uint32_t aInputFormat,
                               const nsAString& aOutputOptions) {
  bool useTransparency = true, skipFirstFrame = false;
  uint32_t numFrames = 1;
  uint32_t numPlays = 0;  // For animations, 0 == forever
  int zlibLevel = DEFAULT_ZLIB_LEVEL;
  int filters = DEFAULT_FILTERS;

  // can't initialize more than once
  if (mImageBuffer != nullptr) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  // validate input format
  if (aInputFormat != INPUT_FORMAT_RGB && aInputFormat != INPUT_FORMAT_RGBA &&
      aInputFormat != INPUT_FORMAT_HOSTARGB)
    return NS_ERROR_INVALID_ARG;

  // parse and check any provided output options
  nsresult rv = ParseOptions(aOutputOptions, &useTransparency, &skipFirstFrame,
                             &numFrames, &numPlays, &zlibLevel, &filters,
                             nullptr, nullptr, nullptr, nullptr, nullptr);
  if (rv != NS_OK) {
    return rv;
  }

#ifdef PNG_APNG_SUPPORTED
  if (numFrames > 1) {
    mIsAnimation = true;
  }

#endif

  // initialize
  mPNG = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, ErrorCallback,
                                 WarningCallback);
  if (!mPNG) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mPNGinfo = png_create_info_struct(mPNG);
  if (!mPNGinfo) {
    png_destroy_write_struct(&mPNG, nullptr);
    return NS_ERROR_FAILURE;
  }

  // libpng's error handler jumps back here upon an error.
  // Note: It's important that all png_* callers do this, or errors
  // will result in a corrupt time-warped stack.
  if (setjmp(png_jmpbuf(mPNG))) {
    png_destroy_write_struct(&mPNG, &mPNGinfo);
    return NS_ERROR_FAILURE;
  }

#ifdef PNG_WRITE_CUSTOMIZE_COMPRESSION_SUPPORTED
  png_set_compression_level(mPNG, zlibLevel);
#endif
#ifdef PNG_WRITE_FILTER_SUPPORTED
  png_set_filter(mPNG, PNG_FILTER_TYPE_BASE, filters);
#endif

  // Set up to read the data into our image buffer, start out with an 8K
  // estimated size. Note: we don't have to worry about freeing this data
  // in this function. It will be freed on object destruction.
  mImageBufferSize = 8192;
  mImageBuffer = (uint8_t*)malloc(mImageBufferSize);
  if (!mImageBuffer) {
    png_destroy_write_struct(&mPNG, &mPNGinfo);
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mImageBufferUsed = 0;

  // set our callback for libpng to give us the data
  png_set_write_fn(mPNG, this, WriteCallback, nullptr);

  // include alpha?
  int colorType;
  if ((aInputFormat == INPUT_FORMAT_HOSTARGB ||
       aInputFormat == INPUT_FORMAT_RGBA) &&
      useTransparency)
    colorType = PNG_COLOR_TYPE_RGB_ALPHA;
  else
    colorType = PNG_COLOR_TYPE_RGB;

  png_set_IHDR(mPNG, mPNGinfo, aWidth, aHeight, 8, colorType,
               PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
               PNG_FILTER_TYPE_DEFAULT);

#ifdef PNG_APNG_SUPPORTED
  if (mIsAnimation) {
    png_set_first_frame_is_hidden(mPNG, mPNGinfo, skipFirstFrame);
    png_set_acTL(mPNG, mPNGinfo, numFrames, numPlays);
  }
#endif

  // XXX: support PLTE, gAMA, tRNS, bKGD?

  png_write_info(mPNG, mPNGinfo);

  return NS_OK;
}

// Returns the number of bytes in the image buffer used.
NS_IMETHODIMP
nsPNGEncoder::GetImageBufferUsed(uint32_t* aOutputSize) {
  NS_ENSURE_ARG_POINTER(aOutputSize);
  *aOutputSize = mImageBufferUsed;
  return NS_OK;
}

// Returns a pointer to the start of the image buffer
NS_IMETHODIMP
nsPNGEncoder::GetImageBuffer(char** aOutputBuffer) {
  NS_ENSURE_ARG_POINTER(aOutputBuffer);
  *aOutputBuffer = reinterpret_cast<char*>(mImageBuffer);
  return NS_OK;
}

NS_IMETHODIMP
nsPNGEncoder::AddImageFrame(const uint8_t* aData,
                            uint32_t aLength,  // (unused, req'd by JS)
                            uint32_t aWidth, uint32_t aHeight, uint32_t aStride,
                            uint32_t aInputFormat,
                            const nsAString& aFrameOptions) {
  bool useTransparency = true;
  int filters = DEFAULT_FILTERS;
  uint32_t delay_ms = 500;
#ifdef PNG_APNG_SUPPORTED
  uint32_t dispose_op = PNG_DISPOSE_OP_NONE;
  uint32_t blend_op = PNG_BLEND_OP_SOURCE;
#else
  uint32_t dispose_op;
  uint32_t blend_op;
#endif
  uint32_t x_offset = 0, y_offset = 0;

  // must be initialized
  if (mImageBuffer == nullptr) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // EndImageEncode was done, or some error occurred earlier
  if (!mPNG) {
    return NS_BASE_STREAM_CLOSED;
  }

  // validate input format
  if (aInputFormat != INPUT_FORMAT_RGB && aInputFormat != INPUT_FORMAT_RGBA &&
      aInputFormat != INPUT_FORMAT_HOSTARGB)
    return NS_ERROR_INVALID_ARG;

  // libpng's error handler jumps back here upon an error.
  if (setjmp(png_jmpbuf(mPNG))) {
    png_destroy_write_struct(&mPNG, &mPNGinfo);
    return NS_ERROR_FAILURE;
  }

  // parse and check any provided output options
  nsresult rv = ParseOptions(aFrameOptions, &useTransparency, nullptr, nullptr,
                             nullptr, nullptr, &filters, &dispose_op, &blend_op,
                             &delay_ms, &x_offset, &y_offset);
  if (rv != NS_OK) {
    return rv;
  }

#ifdef PNG_APNG_SUPPORTED
  if (mIsAnimation) {
    // XXX the row pointers arg (#3) is unused, can it be removed?
    png_write_frame_head(mPNG, mPNGinfo, nullptr, aWidth, aHeight, x_offset,
                         y_offset, delay_ms, 1000, dispose_op, blend_op);
  }
#endif

  // Stride is the padded width of each row, so it better be longer
  // (I'm afraid people will not understand what stride means, so
  // check it well)
  if ((aInputFormat == INPUT_FORMAT_RGB && aStride < aWidth * 3) ||
      ((aInputFormat == INPUT_FORMAT_RGBA ||
        aInputFormat == INPUT_FORMAT_HOSTARGB) &&
       aStride < aWidth * 4)) {
    NS_WARNING("Invalid stride for InitFromData/AddImageFrame");
    return NS_ERROR_INVALID_ARG;
  }

#ifdef PNG_WRITE_FILTER_SUPPORTED
  png_set_filter(mPNG, PNG_FILTER_TYPE_BASE, filters);
#endif

  // write each row: if we add more input formats, we may want to
  // generalize the conversions
  if (aInputFormat == INPUT_FORMAT_HOSTARGB) {
    // PNG requires RGBA with post-multiplied alpha, so we need to
    // convert
    UniquePtr<uint8_t[]> row = MakeUniqueFallible<uint8_t[]>(aWidth * 4);
    if (NS_WARN_IF(!row)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    for (uint32_t y = 0; y < aHeight; y++) {
      ConvertHostARGBRow(&aData[y * aStride], row.get(), aWidth,
                         useTransparency);
      png_write_row(mPNG, row.get());
    }
  } else if (aInputFormat == INPUT_FORMAT_RGBA && !useTransparency) {
    // RBGA, but we need to strip the alpha
    UniquePtr<uint8_t[]> row = MakeUniqueFallible<uint8_t[]>(aWidth * 4);
    if (NS_WARN_IF(!row)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    for (uint32_t y = 0; y < aHeight; y++) {
      StripAlpha(&aData[y * aStride], row.get(), aWidth);
      png_write_row(mPNG, row.get());
    }
  } else if (aInputFormat == INPUT_FORMAT_RGB ||
             aInputFormat == INPUT_FORMAT_RGBA) {
    // simple RBG(A), no conversion needed
    for (uint32_t y = 0; y < aHeight; y++) {
      png_write_row(mPNG, (uint8_t*)&aData[y * aStride]);
    }

  } else {
    MOZ_ASSERT_UNREACHABLE("Bad format type");
    return NS_ERROR_INVALID_ARG;
  }

#ifdef PNG_APNG_SUPPORTED
  if (mIsAnimation) {
    png_write_frame_tail(mPNG, mPNGinfo);
  }
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsPNGEncoder::EndImageEncode() {
  // must be initialized
  if (mImageBuffer == nullptr) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // EndImageEncode has already been called, or some error
  // occurred earlier
  if (!mPNG) {
    return NS_BASE_STREAM_CLOSED;
  }

  // libpng's error handler jumps back here upon an error.
  if (setjmp(png_jmpbuf(mPNG))) {
    png_destroy_write_struct(&mPNG, &mPNGinfo);
    return NS_ERROR_FAILURE;
  }

  png_write_end(mPNG, mPNGinfo);
  png_destroy_write_struct(&mPNG, &mPNGinfo);

  mFinished = true;
  NotifyListener();

  // if output callback can't get enough memory, it will free our buffer
  if (!mImageBuffer) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

nsresult nsPNGEncoder::ParseOptions(const nsAString& aOptions,
                                    bool* useTransparency, bool* skipFirstFrame,
                                    uint32_t* numFrames, uint32_t* numPlays,
                                    int* zlibLevel, int* filters,
                                    uint32_t* frameDispose,
                                    uint32_t* frameBlend, uint32_t* frameDelay,
                                    uint32_t* offsetX, uint32_t* offsetY) {
#ifdef PNG_APNG_SUPPORTED
  // Make a copy of aOptions, because strtok() will modify it.
  nsAutoCString optionsCopy;
  optionsCopy.Assign(NS_ConvertUTF16toUTF8(aOptions));
  char* options = optionsCopy.BeginWriting();

  while (char* token = nsCRT::strtok(options, ";", &options)) {
    // If there's an '=' character, split the token around it.
    char* equals = token;
    char* value = nullptr;
    while (*equals != '=' && *equals) {
      ++equals;
    }
    if (*equals == '=') {
      value = equals + 1;
    }

    if (value) {
      *equals = '\0';  // temporary null
    }

    // transparency=[yes|no|none]
    if (nsCRT::strcmp(token, "transparency") == 0 && useTransparency) {
      if (!value) {
        return NS_ERROR_INVALID_ARG;
      }

      if (nsCRT::strcmp(value, "none") == 0 ||
          nsCRT::strcmp(value, "no") == 0) {
        *useTransparency = false;
      } else if (nsCRT::strcmp(value, "yes") == 0) {
        *useTransparency = true;
      } else {
        return NS_ERROR_INVALID_ARG;
      }

      // skipfirstframe=[yes|no]
    } else if (nsCRT::strcmp(token, "skipfirstframe") == 0 && skipFirstFrame) {
      if (!value) {
        return NS_ERROR_INVALID_ARG;
      }

      if (nsCRT::strcmp(value, "no") == 0) {
        *skipFirstFrame = false;
      } else if (nsCRT::strcmp(value, "yes") == 0) {
        *skipFirstFrame = true;
      } else {
        return NS_ERROR_INVALID_ARG;
      }

      // frames=#
    } else if (nsCRT::strcmp(token, "frames") == 0 && numFrames) {
      if (!value) {
        return NS_ERROR_INVALID_ARG;
      }

      if (PR_sscanf(value, "%u", numFrames) != 1) {
        return NS_ERROR_INVALID_ARG;
      }

      // frames=0 is nonsense.
      if (*numFrames == 0) {
        return NS_ERROR_INVALID_ARG;
      }

      // plays=#
    } else if (nsCRT::strcmp(token, "plays") == 0 && numPlays) {
      if (!value) {
        return NS_ERROR_INVALID_ARG;
      }

      // plays=0 to loop forever, otherwise play sequence specified
      // number of times
      if (PR_sscanf(value, "%u", numPlays) != 1) {
        return NS_ERROR_INVALID_ARG;
      }

      // png-zlib-level=#
    } else if (nsCRT::strcmp(token, "png-zlib-level") == 0) {
      if (!value) {
        return NS_ERROR_INVALID_ARG;
      }

      int localZlibLevel = DEFAULT_ZLIB_LEVEL;
      if (PR_sscanf(value, "%d", &localZlibLevel) != 1) {
        return NS_ERROR_INVALID_ARG;
      }

      // zlib-level 0-9 are the only valid values
      if (localZlibLevel < 0 || localZlibLevel > 9) {
        return NS_ERROR_INVALID_ARG;
      }

      if (zlibLevel) {
        *zlibLevel = localZlibLevel;
      }

      // png-filter=[no_filters|none|sub|up|avg|paeth|fast|all]
    } else if (nsCRT::strcmp(token, "png-filter") == 0) {
      if (!value) {
        return NS_ERROR_INVALID_ARG;
      }

      if (nsCRT::strcmp(value, "no_filters") == 0) {
        if (filters) {
          *filters = PNG_NO_FILTERS;
        }
      } else if (nsCRT::strcmp(value, "none") == 0) {
        if (filters) {
          *filters = PNG_FILTER_NONE;
        }
      } else if (nsCRT::strcmp(value, "sub") == 0) {
        if (filters) {
          *filters = PNG_FILTER_SUB;
        }
      } else if (nsCRT::strcmp(value, "up") == 0) {
        if (filters) {
          *filters = PNG_FILTER_UP;
        }
      } else if (nsCRT::strcmp(value, "avg") == 0) {
        if (filters) {
          *filters = PNG_FILTER_AVG;
        }
      } else if (nsCRT::strcmp(value, "paeth") == 0) {
        if (filters) {
          *filters = PNG_FILTER_PAETH;
        }
      } else if (nsCRT::strcmp(value, "fast") == 0) {
        if (filters) {
          *filters = PNG_FAST_FILTERS;
        }
      } else if (nsCRT::strcmp(value, "all") == 0) {
        if (filters) {
          *filters = PNG_ALL_FILTERS;
        }
      } else {
        return NS_ERROR_INVALID_ARG;
      }

      // dispose=[none|background|previous]
    } else if (nsCRT::strcmp(token, "dispose") == 0 && frameDispose) {
      if (!value) {
        return NS_ERROR_INVALID_ARG;
      }

      if (nsCRT::strcmp(value, "none") == 0) {
        *frameDispose = PNG_DISPOSE_OP_NONE;
      } else if (nsCRT::strcmp(value, "background") == 0) {
        *frameDispose = PNG_DISPOSE_OP_BACKGROUND;
      } else if (nsCRT::strcmp(value, "previous") == 0) {
        *frameDispose = PNG_DISPOSE_OP_PREVIOUS;
      } else {
        return NS_ERROR_INVALID_ARG;
      }

      // blend=[source|over]
    } else if (nsCRT::strcmp(token, "blend") == 0 && frameBlend) {
      if (!value) {
        return NS_ERROR_INVALID_ARG;
      }

      if (nsCRT::strcmp(value, "source") == 0) {
        *frameBlend = PNG_BLEND_OP_SOURCE;
      } else if (nsCRT::strcmp(value, "over") == 0) {
        *frameBlend = PNG_BLEND_OP_OVER;
      } else {
        return NS_ERROR_INVALID_ARG;
      }

      // delay=# (in ms)
    } else if (nsCRT::strcmp(token, "delay") == 0 && frameDelay) {
      if (!value) {
        return NS_ERROR_INVALID_ARG;
      }

      if (PR_sscanf(value, "%u", frameDelay) != 1) {
        return NS_ERROR_INVALID_ARG;
      }

      // xoffset=#
    } else if (nsCRT::strcmp(token, "xoffset") == 0 && offsetX) {
      if (!value) {
        return NS_ERROR_INVALID_ARG;
      }

      if (PR_sscanf(value, "%u", offsetX) != 1) {
        return NS_ERROR_INVALID_ARG;
      }

      // yoffset=#
    } else if (nsCRT::strcmp(token, "yoffset") == 0 && offsetY) {
      if (!value) {
        return NS_ERROR_INVALID_ARG;
      }

      if (PR_sscanf(value, "%u", offsetY) != 1) {
        return NS_ERROR_INVALID_ARG;
      }

      // unknown token name
    } else
      return NS_ERROR_INVALID_ARG;

    if (value) {
      *equals = '=';  // restore '=' so strtok doesn't get lost
    }
  }

#endif
  return NS_OK;
}

NS_IMETHODIMP
nsPNGEncoder::Close() {
  if (mImageBuffer != nullptr) {
    free(mImageBuffer);
    mImageBuffer = nullptr;
    mImageBufferSize = 0;
    mImageBufferUsed = 0;
    mImageBufferReadPoint = 0;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsPNGEncoder::Available(uint64_t* _retval) {
  if (!mImageBuffer) {
    return NS_BASE_STREAM_CLOSED;
  }

  *_retval = mImageBufferUsed - mImageBufferReadPoint;
  return NS_OK;
}

NS_IMETHODIMP
nsPNGEncoder::StreamStatus() {
  return mImageBuffer ? NS_OK : NS_BASE_STREAM_CLOSED;
}

NS_IMETHODIMP
nsPNGEncoder::Read(char* aBuf, uint32_t aCount, uint32_t* _retval) {
  return ReadSegments(NS_CopySegmentToBuffer, aBuf, aCount, _retval);
}

NS_IMETHODIMP
nsPNGEncoder::ReadSegments(nsWriteSegmentFun aWriter, void* aClosure,
                           uint32_t aCount, uint32_t* _retval) {
  // Avoid another thread reallocing the buffer underneath us
  ReentrantMonitorAutoEnter autoEnter(mReentrantMonitor);

  uint32_t maxCount = mImageBufferUsed - mImageBufferReadPoint;
  if (maxCount == 0) {
    *_retval = 0;
    return mFinished ? NS_OK : NS_BASE_STREAM_WOULD_BLOCK;
  }

  if (aCount > maxCount) {
    aCount = maxCount;
  }

  nsresult rv = aWriter(
      this, aClosure,
      reinterpret_cast<const char*>(mImageBuffer + mImageBufferReadPoint), 0,
      aCount, _retval);
  if (NS_SUCCEEDED(rv)) {
    NS_ASSERTION(*_retval <= aCount, "bad write count");
    mImageBufferReadPoint += *_retval;
  }

  // errors returned from the writer end here!
  return NS_OK;
}

NS_IMETHODIMP
nsPNGEncoder::IsNonBlocking(bool* _retval) {
  *_retval = true;
  return NS_OK;
}

NS_IMETHODIMP
nsPNGEncoder::AsyncWait(nsIInputStreamCallback* aCallback, uint32_t aFlags,
                        uint32_t aRequestedCount, nsIEventTarget* aTarget) {
  if (aFlags != 0) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  if (mCallback || mCallbackTarget) {
    return NS_ERROR_UNEXPECTED;
  }

  mCallbackTarget = aTarget;
  // 0 means "any number of bytes except 0"
  mNotifyThreshold = aRequestedCount;
  if (!aRequestedCount) {
    mNotifyThreshold = 1024;  // We don't want to notify incessantly
  }

  // We set the callback absolutely last, because NotifyListener uses it to
  // determine if someone needs to be notified.  If we don't set it last,
  // NotifyListener might try to fire off a notification to a null target
  // which will generally cause non-threadsafe objects to be used off the main
  // thread
  mCallback = aCallback;

  // What we are being asked for may be present already
  NotifyListener();
  return NS_OK;
}

NS_IMETHODIMP
nsPNGEncoder::CloseWithStatus(nsresult aStatus) { return Close(); }

// nsPNGEncoder::ConvertHostARGBRow
//
//    Our colors are stored with premultiplied alphas, but PNGs use
//    post-multiplied alpha. This swaps to PNG-style alpha.
//
//    Copied from gfx/cairo/cairo/src/cairo-png.c

void nsPNGEncoder::ConvertHostARGBRow(const uint8_t* aSrc, uint8_t* aDest,
                                      uint32_t aPixelWidth,
                                      bool aUseTransparency) {
  uint32_t pixelStride = aUseTransparency ? 4 : 3;
  for (uint32_t x = 0; x < aPixelWidth; x++) {
    const uint32_t& pixelIn = ((const uint32_t*)(aSrc))[x];
    uint8_t* pixelOut = &aDest[x * pixelStride];

    uint8_t alpha = (pixelIn & 0xff000000) >> 24;
    pixelOut[pixelStride - 1] = alpha;  // overwritten below if pixelStride == 3
    if (alpha == 255) {
      pixelOut[0] = (pixelIn & 0xff0000) >> 16;
      pixelOut[1] = (pixelIn & 0x00ff00) >> 8;
      pixelOut[2] = (pixelIn & 0x0000ff);
    } else if (alpha == 0) {
      pixelOut[0] = pixelOut[1] = pixelOut[2] = 0;
    } else {
      pixelOut[0] = (((pixelIn & 0xff0000) >> 16) * 255 + alpha / 2) / alpha;
      pixelOut[1] = (((pixelIn & 0x00ff00) >> 8) * 255 + alpha / 2) / alpha;
      pixelOut[2] = (((pixelIn & 0x0000ff)) * 255 + alpha / 2) / alpha;
    }
  }
}

// nsPNGEncoder::StripAlpha
//
//    Input is RGBA, output is RGB

void nsPNGEncoder::StripAlpha(const uint8_t* aSrc, uint8_t* aDest,
                              uint32_t aPixelWidth) {
  for (uint32_t x = 0; x < aPixelWidth; x++) {
    const uint8_t* pixelIn = &aSrc[x * 4];
    uint8_t* pixelOut = &aDest[x * 3];
    pixelOut[0] = pixelIn[0];
    pixelOut[1] = pixelIn[1];
    pixelOut[2] = pixelIn[2];
  }
}

// nsPNGEncoder::WarningCallback

void nsPNGEncoder::WarningCallback(png_structp png_ptr,
                                   png_const_charp warning_msg) {
  MOZ_LOG(sPNGEncoderLog, LogLevel::Warning,
          ("libpng warning: %s\n", warning_msg));
}

// nsPNGEncoder::ErrorCallback

void nsPNGEncoder::ErrorCallback(png_structp png_ptr,
                                 png_const_charp error_msg) {
  MOZ_LOG(sPNGEncoderLog, LogLevel::Error, ("libpng error: %s\n", error_msg));
  png_longjmp(png_ptr, 1);
}

// nsPNGEncoder::WriteCallback

void  // static
nsPNGEncoder::WriteCallback(png_structp png, png_bytep data, png_size_t size) {
  nsPNGEncoder* that = static_cast<nsPNGEncoder*>(png_get_io_ptr(png));
  if (!that->mImageBuffer) {
    return;
  }

  CheckedUint32 sizeNeeded = CheckedUint32(that->mImageBufferUsed) + size;
  if (!sizeNeeded.isValid()) {
    // Take the lock to ensure that nobody is trying to read from the buffer
    // we are destroying
    ReentrantMonitorAutoEnter autoEnter(that->mReentrantMonitor);

    that->NullOutImageBuffer();
    return;
  }

  if (sizeNeeded.value() > that->mImageBufferSize) {
    // When we're reallocing the buffer we need to take the lock to ensure
    // that nobody is trying to read from the buffer we are destroying
    ReentrantMonitorAutoEnter autoEnter(that->mReentrantMonitor);

    while (sizeNeeded.value() > that->mImageBufferSize) {
      // expand buffer, just double each time
      CheckedUint32 bufferSize = CheckedUint32(that->mImageBufferSize) * 2;
      if (!bufferSize.isValid()) {
        that->NullOutImageBuffer();
        return;
      }
      that->mImageBufferSize *= 2;
      uint8_t* newBuf =
          (uint8_t*)realloc(that->mImageBuffer, that->mImageBufferSize);
      if (!newBuf) {
        // can't resize, just zero (this will keep us from writing more)
        that->NullOutImageBuffer();
        return;
      }
      that->mImageBuffer = newBuf;
    }
  }

  memcpy(&that->mImageBuffer[that->mImageBufferUsed], data, size);
  that->mImageBufferUsed += size;
  that->NotifyListener();
}

void nsPNGEncoder::NullOutImageBuffer() {
  mReentrantMonitor.AssertCurrentThreadIn();

  free(mImageBuffer);
  mImageBuffer = nullptr;
  mImageBufferSize = 0;
  mImageBufferUsed = 0;
}

void nsPNGEncoder::NotifyListener() {
  // We might call this function on multiple threads (any threads that call
  // AsyncWait and any that do encoding) so we lock to avoid notifying the
  // listener twice about the same data (which generally leads to a truncated
  // image).
  ReentrantMonitorAutoEnter autoEnter(mReentrantMonitor);

  if (mCallback &&
      (mImageBufferUsed - mImageBufferReadPoint >= mNotifyThreshold ||
       mFinished)) {
    nsCOMPtr<nsIInputStreamCallback> callback;
    if (mCallbackTarget) {
      callback = NS_NewInputStreamReadyEvent("nsPNGEncoder::NotifyListener",
                                             mCallback, mCallbackTarget);
    } else {
      callback = mCallback;
    }

    NS_ASSERTION(callback, "Shouldn't fail to make the callback");
    // Null the callback first because OnInputStreamReady could reenter
    // AsyncWait
    mCallback = nullptr;
    mCallbackTarget = nullptr;
    mNotifyThreshold = 0;

    callback->OnInputStreamReady(this);
  }
}
