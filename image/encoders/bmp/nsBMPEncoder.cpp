/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCRT.h"
#include "EndianMacros.h"
#include "nsBMPEncoder.h"
#include "prmem.h"
#include "prprf.h"
#include "nsString.h"
#include "nsStreamUtils.h"
#include "nsAutoPtr.h"

using namespace mozilla;

NS_IMPL_THREADSAFE_ISUPPORTS3(nsBMPEncoder, imgIEncoder, nsIInputStream, nsIAsyncInputStream)

nsBMPEncoder::nsBMPEncoder() : mImageBufferStart(nullptr), 
                               mImageBufferCurr(0),
                               mImageBufferSize(0), 
                               mImageBufferReadPoint(0), 
                               mFinished(false),
                               mCallback(nullptr), 
                               mCallbackTarget(nullptr), 
                               mNotifyThreshold(0)
{
}

nsBMPEncoder::~nsBMPEncoder()
{
  if (mImageBufferStart) {
    moz_free(mImageBufferStart);
    mImageBufferStart = nullptr;
    mImageBufferCurr = nullptr;
  }
}

// nsBMPEncoder::InitFromData
//
// One output option is supported: bpp=<bpp_value>
// bpp specifies the bits per pixel to use where bpp_value can be 24 or 32
NS_IMETHODIMP nsBMPEncoder::InitFromData(const PRUint8* aData,
                                         PRUint32 aLength, // (unused,
                                                           // req'd by JS)
                                         PRUint32 aWidth,
                                         PRUint32 aHeight,
                                         PRUint32 aStride,
                                         PRUint32 aInputFormat,
                                         const nsAString& aOutputOptions)
{
  // validate input format
  if (aInputFormat != INPUT_FORMAT_RGB &&
      aInputFormat != INPUT_FORMAT_RGBA &&
      aInputFormat != INPUT_FORMAT_HOSTARGB) {
    return NS_ERROR_INVALID_ARG;
  }

  // Stride is the padded width of each row, so it better be longer
  if ((aInputFormat == INPUT_FORMAT_RGB &&
       aStride < aWidth * 3) ||
      ((aInputFormat == INPUT_FORMAT_RGBA || aInputFormat == INPUT_FORMAT_HOSTARGB) &&
       aStride < aWidth * 4)) {
      NS_WARNING("Invalid stride for InitFromData");
      return NS_ERROR_INVALID_ARG;
  }

  nsresult rv;
  rv = StartImageEncode(aWidth, aHeight, aInputFormat, aOutputOptions);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = AddImageFrame(aData, aLength, aWidth, aHeight, aStride,
                     aInputFormat, aOutputOptions);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = EndImageEncode();
  return rv;
}

// Just a helper method to make it explicit in calculations that we are dealing
// with bytes and not bits
static inline PRUint32
BytesPerPixel(PRUint32 aBPP)
{
  return aBPP / 8;
}

// Calculates the number of padding bytes that are needed per row of image data
static inline PRUint32
PaddingBytes(PRUint32 aBPP, PRUint32 aWidth)
{
  PRUint32 rowSize = aWidth * BytesPerPixel(aBPP);
  PRUint8 paddingSize = 0;
  if(rowSize % 4) {
    paddingSize = (4 - (rowSize % 4));
  }
  return paddingSize;
}

// See ::InitFromData for other info.
NS_IMETHODIMP nsBMPEncoder::StartImageEncode(PRUint32 aWidth,
                                             PRUint32 aHeight,
                                             PRUint32 aInputFormat,
                                             const nsAString& aOutputOptions)
{
  // can't initialize more than once
  if (mImageBufferStart || mImageBufferCurr) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  // validate input format
  if (aInputFormat != INPUT_FORMAT_RGB &&
      aInputFormat != INPUT_FORMAT_RGBA &&
      aInputFormat != INPUT_FORMAT_HOSTARGB) {
    return NS_ERROR_INVALID_ARG;
  }

  // parse and check any provided output options
  Version version;
  PRUint32 bpp;
  nsresult rv = ParseOptions(aOutputOptions, &version, &bpp);
  if (NS_FAILED(rv)) {
    return rv;
  }

  InitFileHeader(version, bpp, aWidth, aHeight);
  InitInfoHeader(version, bpp, aWidth, aHeight);

  mImageBufferSize = mBMPFileHeader.filesize;
  mImageBufferStart = static_cast<PRUint8*>(moz_malloc(mImageBufferSize));
  if (!mImageBufferStart) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mImageBufferCurr = mImageBufferStart;

  EncodeFileHeader();
  EncodeInfoHeader();

  return NS_OK;
}

// Returns the number of bytes in the image buffer used.
// For a BMP file, this is all bytes in the buffer.
NS_IMETHODIMP nsBMPEncoder::GetImageBufferUsed(PRUint32 *aOutputSize)
{
  NS_ENSURE_ARG_POINTER(aOutputSize);
  *aOutputSize = mImageBufferSize;
  return NS_OK;
}

// Returns a pointer to the start of the image buffer
NS_IMETHODIMP nsBMPEncoder::GetImageBuffer(char **aOutputBuffer)
{
  NS_ENSURE_ARG_POINTER(aOutputBuffer);
  *aOutputBuffer = reinterpret_cast<char*>(mImageBufferStart);
  return NS_OK;
}

NS_IMETHODIMP nsBMPEncoder::AddImageFrame(const PRUint8* aData,
                                          PRUint32 aLength, // (unused,
                                                            // req'd by JS)
                                          PRUint32 aWidth,
                                          PRUint32 aHeight,
                                          PRUint32 aStride,
                                          PRUint32 aInputFormat,
                                          const nsAString& aFrameOptions)
{
  // must be initialized
  if (!mImageBufferStart || !mImageBufferCurr) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // validate input format
  if (aInputFormat != INPUT_FORMAT_RGB &&
      aInputFormat != INPUT_FORMAT_RGBA &&
      aInputFormat != INPUT_FORMAT_HOSTARGB) {
    return NS_ERROR_INVALID_ARG;
  }

  static fallible_t fallible = fallible_t();
  nsAutoArrayPtr<PRUint8> row(new (fallible) 
                              PRUint8[mBMPInfoHeader.width * 
                              BytesPerPixel(mBMPInfoHeader.bpp)]);
  if (!row) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // write each row: if we add more input formats, we may want to
  // generalize the conversions
  if (aInputFormat == INPUT_FORMAT_HOSTARGB) {
    // BMP requires RGBA with post-multiplied alpha, so we need to convert
    for (PRInt32 y = mBMPInfoHeader.height - 1; y >= 0 ; y --) {
      ConvertHostARGBRow(&aData[y * aStride], row, mBMPInfoHeader.width);
      if(mBMPInfoHeader.bpp == 24) {
        EncodeImageDataRow24(row);
      } else {
        EncodeImageDataRow32(row);
      }
    }
  } else if (aInputFormat == INPUT_FORMAT_RGBA) {
    // simple RGBA, no conversion needed
    for (PRInt32 y = 0; y < mBMPInfoHeader.height; y ++) {
      if (mBMPInfoHeader.bpp == 24) {
        EncodeImageDataRow24(row);
      } else {
        EncodeImageDataRow32(row);
      }
    }
  } else if (aInputFormat == INPUT_FORMAT_RGB) {
    // simple RGB, no conversion needed
    for (PRInt32 y = 0; y < mBMPInfoHeader.height; y ++) {
      if (mBMPInfoHeader.bpp == 24) {
        EncodeImageDataRow24(&aData[y * aStride]);
      } else { 
        EncodeImageDataRow32(&aData[y * aStride]);
      }
    }
  } else {
    NS_NOTREACHED("Bad format type");
    return NS_ERROR_INVALID_ARG;
  }

  return NS_OK;
}


NS_IMETHODIMP nsBMPEncoder::EndImageEncode()
{
  // must be initialized
  if (!mImageBufferStart || !mImageBufferCurr) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  mFinished = true;
  NotifyListener();

  // if output callback can't get enough memory, it will free our buffer
  if (!mImageBufferStart || !mImageBufferCurr) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}


// Parses the encoder options and sets the bits per pixel to use
// See InitFromData for a description of the parse options
nsresult
nsBMPEncoder::ParseOptions(const nsAString& aOptions, Version* version,
                           PRUint32* bpp)
{
  if (version) {
    *version = VERSION_3;
  }
  if (bpp) {
    *bpp = 24;
  }
  
  // Parse the input string into a set of name/value pairs.
  // From a format like: name=value;bpp=<bpp_value>;name=value
  // to format: [0] = name=value, [1] = bpp=<bpp_value>, [2] = name=value
  nsTArray<nsCString> nameValuePairs;
  if (!ParseString(NS_ConvertUTF16toUTF8(aOptions), ';', nameValuePairs)) {
    return NS_ERROR_INVALID_ARG;
  }

  // For each name/value pair in the set
  for (PRUint32 i = 0; i < nameValuePairs.Length(); ++i) {

    // Split the name value pair [0] = name, [1] = value
    nsTArray<nsCString> nameValuePair;
    if (!ParseString(nameValuePairs[i], '=', nameValuePair)) {
      return NS_ERROR_INVALID_ARG;
    }
    if (nameValuePair.Length() != 2) {
      return NS_ERROR_INVALID_ARG;
    }

    // Parse the bpp portion of the string name=value;version=<version_value>;
    // name=value
    if (nameValuePair[0].Equals("version",
                                nsCaseInsensitiveCStringComparator())) {
      if (nameValuePair[1].Equals("3")) {
        *version = VERSION_3;
      } else if (nameValuePair[1].Equals("5")) {
        *version = VERSION_5;
      } else {
        return NS_ERROR_INVALID_ARG;
      }
    }

    // Parse the bpp portion of the string name=value;bpp=<bpp_value>;name=value
    if (nameValuePair[0].Equals("bpp", nsCaseInsensitiveCStringComparator())) {
      if (nameValuePair[1].Equals("24")) {
        *bpp = 24;
      } else if (nameValuePair[1].Equals("32")) {
        *bpp = 32;
      } else {
        return NS_ERROR_INVALID_ARG;
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsBMPEncoder::Close()
{
  if (mImageBufferStart) {
    moz_free(mImageBufferStart);
    mImageBufferStart = nullptr;
    mImageBufferSize = 0;
    mImageBufferReadPoint = 0;
    mImageBufferCurr = nullptr;
  }

  return NS_OK;
}

// Obtains the available bytes to read
NS_IMETHODIMP nsBMPEncoder::Available(PRUint32 *_retval)
{
  if (!mImageBufferStart || !mImageBufferCurr) {
    return NS_BASE_STREAM_CLOSED;
  }

  *_retval = GetCurrentImageBufferOffset() - mImageBufferReadPoint;
  return NS_OK;
}

// [noscript] Reads bytes which are available
NS_IMETHODIMP nsBMPEncoder::Read(char * aBuf, PRUint32 aCount,
                                 PRUint32 *_retval)
{
  return ReadSegments(NS_CopySegmentToBuffer, aBuf, aCount, _retval);
}

// [noscript] Reads segments
NS_IMETHODIMP nsBMPEncoder::ReadSegments(nsWriteSegmentFun aWriter,
                                         void *aClosure, PRUint32 aCount,
                                         PRUint32 *_retval)
{
  PRUint32 maxCount = GetCurrentImageBufferOffset() - mImageBufferReadPoint;
  if (maxCount == 0) {
    *_retval = 0;
    return mFinished ? NS_OK : NS_BASE_STREAM_WOULD_BLOCK;
  }

  if (aCount > maxCount) {
    aCount = maxCount;
  }
  nsresult rv = aWriter(this, aClosure,
                        reinterpret_cast<const char*>(mImageBufferStart + 
                                                      mImageBufferReadPoint),
                        0, aCount, _retval);
  if (NS_SUCCEEDED(rv)) {
    NS_ASSERTION(*_retval <= aCount, "bad write count");
    mImageBufferReadPoint += *_retval;
  }
  // errors returned from the writer end here!
  return NS_OK;
}

NS_IMETHODIMP 
nsBMPEncoder::IsNonBlocking(bool *_retval)
{
  *_retval = true;
  return NS_OK;
}

NS_IMETHODIMP 
nsBMPEncoder::AsyncWait(nsIInputStreamCallback *aCallback,
                        PRUint32 aFlags,
                        PRUint32 aRequestedCount,
                        nsIEventTarget *aTarget)
{
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
    mNotifyThreshold = 1024; // We don't want to notify incessantly
  }

  // We set the callback absolutely last, because NotifyListener uses it to
  // determine if someone needs to be notified.  If we don't set it last,
  // NotifyListener might try to fire off a notification to a null target
  // which will generally cause non-threadsafe objects to be used off the main thread
  mCallback = aCallback;

  // What we are being asked for may be present already
  NotifyListener();
  return NS_OK;
}

NS_IMETHODIMP nsBMPEncoder::CloseWithStatus(nsresult aStatus)
{
  return Close();
}

// nsBMPEncoder::ConvertHostARGBRow
//
//    Our colors are stored with premultiplied alphas, but we need
//    an output with no alpha in machine-independent byte order.
//
void
nsBMPEncoder::ConvertHostARGBRow(const PRUint8* aSrc, PRUint8* aDest,
                                 PRUint32 aPixelWidth)
{
  int bytes = BytesPerPixel(mBMPInfoHeader.bpp);

  if (mBMPInfoHeader.bpp == 32) {
    for (PRUint32 x = 0; x < aPixelWidth; x++) {
      const PRUint32& pixelIn = ((const PRUint32*)(aSrc))[x];
      PRUint8 *pixelOut = &aDest[x * bytes];

      pixelOut[0] = (pixelIn & 0x00ff0000) >> 16;
      pixelOut[1] = (pixelIn & 0x0000ff00) >>  8;
      pixelOut[2] = (pixelIn & 0x000000ff) >>  0;
      pixelOut[3] = (pixelIn & 0xff000000) >> 24;
    }
  } else {
    for (PRUint32 x = 0; x < aPixelWidth; x++) {
      const PRUint32& pixelIn = ((const PRUint32*)(aSrc))[x];
      PRUint8 *pixelOut = &aDest[x * bytes];

      pixelOut[0] = (pixelIn & 0xff0000) >> 16;
      pixelOut[1] = (pixelIn & 0x00ff00) >>  8;
      pixelOut[2] = (pixelIn & 0x0000ff) >>  0;
    }
  }
}

void
nsBMPEncoder::NotifyListener()
{
  if (mCallback &&
      (GetCurrentImageBufferOffset() - mImageBufferReadPoint >= 
       mNotifyThreshold || mFinished)) {
    nsCOMPtr<nsIInputStreamCallback> callback;
    if (mCallbackTarget) {
      NS_NewInputStreamReadyEvent(getter_AddRefs(callback),
                                  mCallback,
                                  mCallbackTarget);
    } else {
      callback = mCallback;
    }

    NS_ASSERTION(callback, "Shouldn't fail to make the callback");
    // Null the callback first because OnInputStreamReady could
    // reenter AsyncWait
    mCallback = nullptr;
    mCallbackTarget = nullptr;
    mNotifyThreshold = 0;

    callback->OnInputStreamReady(this);
  }
}

// Initializes the BMP file header mBMPFileHeader to the passed in values
void 
nsBMPEncoder::InitFileHeader(Version aVersion, PRUint32 aBPP, PRUint32 aWidth,
                             PRUint32 aHeight)
{
  memset(&mBMPFileHeader, 0, sizeof(mBMPFileHeader));
  mBMPFileHeader.signature[0] = 'B';
  mBMPFileHeader.signature[1] = 'M';
  
  if (aVersion == VERSION_3) {
    mBMPFileHeader.dataoffset = WIN_V3_HEADER_LENGTH;
  } else { // aVersion == 5
    mBMPFileHeader.dataoffset = WIN_V5_HEADER_LENGTH;
  }

  // The color table is present only if BPP is <= 8
  if (aBPP <= 8) {
    PRUint32 numColors = 1 << aBPP;
    mBMPFileHeader.dataoffset += 4 * numColors;
    mBMPFileHeader.filesize = mBMPFileHeader.dataoffset + aWidth * aHeight;
  } else {
    mBMPFileHeader.filesize = mBMPFileHeader.dataoffset + (aWidth * 
                              BytesPerPixel(aBPP) + PaddingBytes(aBPP, aWidth)) *
                              aHeight;
  }

  mBMPFileHeader.reserved = 0;

  if (aVersion == VERSION_3) {
    mBMPFileHeader.bihsize = WIN_V3_BIH_LENGTH;
  } else { // aVersion == VERSION_5
    mBMPFileHeader.bihsize = WIN_V5_BIH_LENGTH;
  }
}

#define ENCODE(pImageBufferCurr, value) \
    memcpy(*pImageBufferCurr, &value, sizeof value); \
    *pImageBufferCurr += sizeof value;

// Initializes the bitmap info header mBMPInfoHeader to the passed in values
void 
nsBMPEncoder::InitInfoHeader(Version aVersion, PRUint32 aBPP, PRUint32 aWidth,
                             PRUint32 aHeight)
{
  memset(&mBMPInfoHeader, 0, sizeof(mBMPInfoHeader));
  mBMPInfoHeader.width = aWidth;
  mBMPInfoHeader.height = aHeight;
  mBMPInfoHeader.planes = 1;
  mBMPInfoHeader.bpp = aBPP;
  mBMPInfoHeader.compression = 0;
  mBMPInfoHeader.colors = 0;
  mBMPInfoHeader.important_colors = 0;
  if (aBPP <= 8) {
    mBMPInfoHeader.image_size = aWidth * aHeight;
  } else {
    mBMPInfoHeader.image_size = (aWidth * BytesPerPixel(aBPP) + 
                                 PaddingBytes(aBPP, aWidth)) * aHeight;
  }
  mBMPInfoHeader.xppm = 0;
  mBMPInfoHeader.yppm = 0;
  if (aVersion >= VERSION_5) {
      mBMPInfoHeader.red_mask   = 0x000000FF;
      mBMPInfoHeader.green_mask = 0x0000FF00;
      mBMPInfoHeader.blue_mask  = 0x00FF0000;
      mBMPInfoHeader.alpha_mask = 0xFF000000;
      mBMPInfoHeader.color_space = LCS_sRGB;
      mBMPInfoHeader.white_point.r.x = 0;
      mBMPInfoHeader.white_point.r.y = 0;
      mBMPInfoHeader.white_point.r.z = 0;
      mBMPInfoHeader.white_point.g.x = 0;
      mBMPInfoHeader.white_point.g.y = 0;
      mBMPInfoHeader.white_point.g.z = 0;
      mBMPInfoHeader.white_point.b.x = 0;
      mBMPInfoHeader.white_point.b.y = 0;
      mBMPInfoHeader.white_point.b.z = 0;
      mBMPInfoHeader.gamma_red = 0;
      mBMPInfoHeader.gamma_green = 0;
      mBMPInfoHeader.gamma_blue = 0;
      mBMPInfoHeader.intent = 0;
      mBMPInfoHeader.profile_offset = 0;
      mBMPInfoHeader.profile_size = 0;
      mBMPInfoHeader.reserved = 0;
  }
}

template<typename T>
static inline void
ConvertToLittle(T& value)
{
    value = NATIVE32_TO_LITTLE(value);
}

// Encodes the BMP file header mBMPFileHeader
void 
nsBMPEncoder::EncodeFileHeader() 
{  
  mozilla::image::BMPFILEHEADER littleEndianBFH = mBMPFileHeader;
  ConvertToLittle(littleEndianBFH.filesize);
  ConvertToLittle(littleEndianBFH.reserved);
  ConvertToLittle(littleEndianBFH.dataoffset);
  ConvertToLittle(littleEndianBFH.bihsize);

  ENCODE(&mImageBufferCurr, littleEndianBFH.signature);
  ENCODE(&mImageBufferCurr, littleEndianBFH.filesize);
  ENCODE(&mImageBufferCurr, littleEndianBFH.reserved);
  ENCODE(&mImageBufferCurr, littleEndianBFH.dataoffset);
  ENCODE(&mImageBufferCurr, littleEndianBFH.bihsize);
}

// Encodes the BMP infor header mBMPInfoHeader
void 
nsBMPEncoder::EncodeInfoHeader()
{
  mozilla::image::BITMAPV5HEADER littleEndianmBIH = mBMPInfoHeader;
  ConvertToLittle(littleEndianmBIH.width);
  ConvertToLittle(littleEndianmBIH.height);
  ConvertToLittle(littleEndianmBIH.planes);
  ConvertToLittle(littleEndianmBIH.bpp);
  ConvertToLittle(littleEndianmBIH.compression);
  ConvertToLittle(littleEndianmBIH.image_size);
  ConvertToLittle(littleEndianmBIH.xppm);
  ConvertToLittle(littleEndianmBIH.yppm);
  ConvertToLittle(littleEndianmBIH.colors);
  ConvertToLittle(littleEndianmBIH.important_colors);
  ConvertToLittle(littleEndianmBIH.red_mask);
  ConvertToLittle(littleEndianmBIH.green_mask);
  ConvertToLittle(littleEndianmBIH.blue_mask);
  ConvertToLittle(littleEndianmBIH.alpha_mask);
  ConvertToLittle(littleEndianmBIH.color_space);
  ConvertToLittle(littleEndianmBIH.white_point.r.x);
  ConvertToLittle(littleEndianmBIH.white_point.r.y);
  ConvertToLittle(littleEndianmBIH.white_point.r.z);
  ConvertToLittle(littleEndianmBIH.white_point.g.x);
  ConvertToLittle(littleEndianmBIH.white_point.g.y);
  ConvertToLittle(littleEndianmBIH.white_point.g.z);
  ConvertToLittle(littleEndianmBIH.white_point.b.x);
  ConvertToLittle(littleEndianmBIH.white_point.b.y);
  ConvertToLittle(littleEndianmBIH.white_point.b.z);
  ConvertToLittle(littleEndianmBIH.gamma_red);
  ConvertToLittle(littleEndianmBIH.gamma_green);
  ConvertToLittle(littleEndianmBIH.gamma_blue);
  ConvertToLittle(littleEndianmBIH.intent);
  ConvertToLittle(littleEndianmBIH.profile_offset);
  ConvertToLittle(littleEndianmBIH.profile_size);
  
  if (mBMPFileHeader.bihsize == OS2_BIH_LENGTH) {
      PRUint16 width = (PRUint16) littleEndianmBIH.width;
      ENCODE(&mImageBufferCurr, width);
      PRUint16 height = (PRUint16) littleEndianmBIH.width;
      ENCODE(&mImageBufferCurr, height);
  } else {
      ENCODE(&mImageBufferCurr, littleEndianmBIH.width);
      ENCODE(&mImageBufferCurr, littleEndianmBIH.height);
  }

  ENCODE(&mImageBufferCurr, littleEndianmBIH.planes);
  ENCODE(&mImageBufferCurr, littleEndianmBIH.bpp);

  if (mBMPFileHeader.bihsize > OS2_BIH_LENGTH) {
    ENCODE(&mImageBufferCurr, littleEndianmBIH.compression);
    ENCODE(&mImageBufferCurr, littleEndianmBIH.image_size);
    ENCODE(&mImageBufferCurr, littleEndianmBIH.xppm);
    ENCODE(&mImageBufferCurr, littleEndianmBIH.yppm);
    ENCODE(&mImageBufferCurr, littleEndianmBIH.colors);
    ENCODE(&mImageBufferCurr, littleEndianmBIH.important_colors);
  }

  if (mBMPFileHeader.bihsize > WIN_V3_BIH_LENGTH) {
    ENCODE(&mImageBufferCurr, littleEndianmBIH.red_mask);
    ENCODE(&mImageBufferCurr, littleEndianmBIH.green_mask);
    ENCODE(&mImageBufferCurr, littleEndianmBIH.blue_mask);
    ENCODE(&mImageBufferCurr, littleEndianmBIH.alpha_mask);
    ENCODE(&mImageBufferCurr, littleEndianmBIH.color_space);
    ENCODE(&mImageBufferCurr, littleEndianmBIH.white_point.r.x);
    ENCODE(&mImageBufferCurr, littleEndianmBIH.white_point.r.y);
    ENCODE(&mImageBufferCurr, littleEndianmBIH.white_point.r.z);
    ENCODE(&mImageBufferCurr, littleEndianmBIH.white_point.g.x);
    ENCODE(&mImageBufferCurr, littleEndianmBIH.white_point.g.y);
    ENCODE(&mImageBufferCurr, littleEndianmBIH.white_point.g.z);
    ENCODE(&mImageBufferCurr, littleEndianmBIH.white_point.b.x);
    ENCODE(&mImageBufferCurr, littleEndianmBIH.white_point.b.y);
    ENCODE(&mImageBufferCurr, littleEndianmBIH.white_point.b.z);
    ENCODE(&mImageBufferCurr, littleEndianmBIH.gamma_red);
    ENCODE(&mImageBufferCurr, littleEndianmBIH.gamma_green);
    ENCODE(&mImageBufferCurr, littleEndianmBIH.gamma_blue);
    ENCODE(&mImageBufferCurr, littleEndianmBIH.intent);
    ENCODE(&mImageBufferCurr, littleEndianmBIH.profile_offset);
    ENCODE(&mImageBufferCurr, littleEndianmBIH.profile_size);
    ENCODE(&mImageBufferCurr, littleEndianmBIH.reserved);
  }
}

// Sets a pixel in the image buffer that doesn't have alpha data
static inline void 
  SetPixel24(PRUint8*& imageBufferCurr, PRUint8 aRed, PRUint8 aGreen, 
  PRUint8 aBlue)
{
  *imageBufferCurr = aBlue;
  *(imageBufferCurr + 1) = aGreen;
  *(imageBufferCurr + 2) = aRed;
}

// Sets a pixel in the image buffer with alpha data
static inline void 
SetPixel32(PRUint8*& imageBufferCurr, PRUint8 aRed, PRUint8 aGreen, 
           PRUint8 aBlue, PRUint8 aAlpha = 0xFF)
{
  *imageBufferCurr = aBlue;
  *(imageBufferCurr + 1) = aGreen;
  *(imageBufferCurr + 2) = aRed;
  *(imageBufferCurr + 3) = aAlpha;
}

// Encodes a row of image data which does not have alpha data
void 
nsBMPEncoder::EncodeImageDataRow24(const PRUint8* aData)
{
  for (PRInt32 x = 0; x < mBMPInfoHeader.width; x++) {
    PRUint32 pos = x * BytesPerPixel(mBMPInfoHeader.bpp);
    SetPixel24(mImageBufferCurr, aData[pos], aData[pos + 1], aData[pos + 2]);
    mImageBufferCurr += BytesPerPixel(mBMPInfoHeader.bpp);
  }
  
  for (PRUint32 x = 0; x < PaddingBytes(mBMPInfoHeader.bpp, 
                                        mBMPInfoHeader.width); x++) {
    *mImageBufferCurr++ = 0;
  }
}

// Encodes a row of image data which does have alpha data
void 
nsBMPEncoder::EncodeImageDataRow32(const PRUint8* aData)
{
  for (PRInt32 x = 0; x < mBMPInfoHeader.width; x ++) {
    PRUint32 pos = x * BytesPerPixel(mBMPInfoHeader.bpp);
    SetPixel32(mImageBufferCurr, aData[pos], aData[pos + 1], 
               aData[pos + 2], aData[pos + 3]);
    mImageBufferCurr += 4;
  }

  for (PRUint32 x = 0; x < PaddingBytes(mBMPInfoHeader.bpp, 
                                        mBMPInfoHeader.width); x ++) {
    *mImageBufferCurr++ = 0;
  }
}
