/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
The Graphics Interchange Format(c) is the copyright property of CompuServe
Incorporated. Only CompuServe Incorporated is authorized to define, redefine,
enhance, alter, modify or change in any way the definition of the format.

CompuServe Incorporated hereby grants a limited, non-exclusive, royalty-free
license for the use of the Graphics Interchange Format(sm) in computer
software; computer software utilizing GIF(sm) must acknowledge ownership of the
Graphics Interchange Format and its Service Mark by CompuServe Incorporated, in
User and Technical Documentation. Computer software utilizing GIF, which is
distributed or may be distributed without User or Technical Documentation must
display to the screen or printer a message acknowledging ownership of the
Graphics Interchange Format and the Service Mark by CompuServe Incorporated; in
this case, the acknowledgement may be displayed in an opening screen or leading
banner, or a closing screen or trailing banner. A message such as the following
may be used:

    "The Graphics Interchange Format(c) is the Copyright property of
    CompuServe Incorporated. GIF(sm) is a Service Mark property of
    CompuServe Incorporated."

For further information, please contact :

    CompuServe Incorporated
    Graphics Technology Department
    5000 Arlington Center Boulevard
    Columbus, Ohio  43220
    U. S. A.

CompuServe Incorporated maintains a mailing list with all those individuals and
organizations who wish to receive copies of this document when it is corrected
or revised. This service is offered free of charge; please provide us with your
mailing address.
*/

#include "nsGIFDecoder2.h"

#include <stddef.h>

#include "imgFrame.h"
#include "mozilla/EndianUtils.h"
#include "nsIInputStream.h"
#include "RasterImage.h"
#include "SurfacePipeFactory.h"

#include "gfxColor.h"
#include "gfxPlatform.h"
#include "qcms.h"
#include <algorithm>
#include "mozilla/Telemetry.h"

using namespace mozilla::gfx;

using std::max;

namespace mozilla {
namespace image {

//////////////////////////////////////////////////////////////////////
// GIF Decoder Implementation

static const size_t GIF_HEADER_LEN = 6;
static const size_t GIF_SCREEN_DESCRIPTOR_LEN = 7;
static const size_t BLOCK_HEADER_LEN = 1;
static const size_t SUB_BLOCK_HEADER_LEN = 1;
static const size_t EXTENSION_HEADER_LEN = 2;
static const size_t GRAPHIC_CONTROL_EXTENSION_LEN = 4;
static const size_t APPLICATION_EXTENSION_LEN = 11;
static const size_t IMAGE_DESCRIPTOR_LEN = 9;

// Masks for reading color table information from packed fields in the screen
// descriptor and image descriptor blocks.
static const uint8_t PACKED_FIELDS_COLOR_TABLE_BIT = 0x80;
static const uint8_t PACKED_FIELDS_INTERLACED_BIT = 0x40;
static const uint8_t PACKED_FIELDS_TABLE_DEPTH_MASK = 0x07;

nsGIFDecoder2::nsGIFDecoder2(RasterImage* aImage)
  : Decoder(aImage)
  , mLexer(Transition::To(State::GIF_HEADER, GIF_HEADER_LEN),
           Transition::TerminateSuccess())
  , mOldColor(0)
  , mCurrentFrameIndex(-1)
  , mColorTablePos(0)
  , mGIFOpen(false)
  , mSawTransparency(false)
{
  // Clear out the structure, excluding the arrays.
  memset(&mGIFStruct, 0, sizeof(mGIFStruct));

  // Initialize as "animate once" in case no NETSCAPE2.0 extension is found.
  mGIFStruct.loop_count = 1;
}

nsGIFDecoder2::~nsGIFDecoder2()
{
  free(mGIFStruct.local_colormap);
}

nsresult
nsGIFDecoder2::FinishInternal()
{
  MOZ_ASSERT(!HasError(), "Shouldn't call FinishInternal after error!");

  // If the GIF got cut off, handle it anyway
  if (!IsMetadataDecode() && mGIFOpen) {
    if (mCurrentFrameIndex == mGIFStruct.images_decoded) {
      EndImageFrame();
    }
    PostDecodeDone(mGIFStruct.loop_count - 1);
    mGIFOpen = false;
  }

  return NS_OK;
}

void
nsGIFDecoder2::FlushImageData()
{
  Maybe<SurfaceInvalidRect> invalidRect = mPipe.TakeInvalidRect();
  if (!invalidRect) {
    return;
  }

  PostInvalidation(invalidRect->mInputSpaceRect,
                   Some(invalidRect->mOutputSpaceRect));
}

//******************************************************************************
// GIF decoder callback methods. Part of public API for GIF2
//******************************************************************************

//******************************************************************************
void
nsGIFDecoder2::BeginGIF()
{
  if (mGIFOpen) {
    return;
  }

  mGIFOpen = true;

  PostSize(mGIFStruct.screen_width, mGIFStruct.screen_height);
}

bool
nsGIFDecoder2::CheckForTransparency(const IntRect& aFrameRect)
{
  // Check if the image has a transparent color in its palette.
  if (mGIFStruct.is_transparent) {
    PostHasTransparency();
    return true;
  }

  if (mGIFStruct.images_decoded > 0) {
    return false;  // We only care about first frame padding below.
  }

  // If we need padding on the first frame, that means we don't draw into part
  // of the image at all. Report that as transparency.
  IntRect imageRect(0, 0, mGIFStruct.screen_width, mGIFStruct.screen_height);
  if (!imageRect.IsEqualEdges(aFrameRect)) {
    PostHasTransparency();
    mSawTransparency = true;  // Make sure we don't optimize it away.
    return true;
  }

  return false;
}

//******************************************************************************
nsresult
nsGIFDecoder2::BeginImageFrame(const IntRect& aFrameRect,
                               uint16_t aDepth,
                               bool aIsInterlaced)
{
  MOZ_ASSERT(HasSize());

  bool hasTransparency = CheckForTransparency(aFrameRect);

  // Make sure there's no animation if we're downscaling.
  MOZ_ASSERT_IF(Size() != OutputSize(), !GetImageMetadata().HasAnimation());

  SurfacePipeFlags pipeFlags = aIsInterlaced
                             ? SurfacePipeFlags::DEINTERLACE
                             : SurfacePipeFlags();

  Maybe<SurfacePipe> pipe;
  if (mGIFStruct.images_decoded == 0) {
    gfx::SurfaceFormat format = hasTransparency ? SurfaceFormat::B8G8R8A8
                                                : SurfaceFormat::B8G8R8X8;

    // The first frame may be displayed progressively.
    pipeFlags |= SurfacePipeFlags::PROGRESSIVE_DISPLAY;

    // The first frame is always decoded into an RGB surface.
    pipe =
      SurfacePipeFactory::CreateSurfacePipe(this, mGIFStruct.images_decoded,
                                            Size(), OutputSize(),
                                            aFrameRect, format, pipeFlags);
  } else {
    // This is an animation frame (and not the first). To minimize the memory
    // usage of animations, the image data is stored in paletted form.
    //
    // We should never use paletted surfaces with a draw target directly, so
    // the only practical difference between B8G8R8A8 and B8G8R8X8 is the
    // cleared pixel value if we get truncated. We want 0 in that case to
    // ensure it is an acceptable value for the color map as was the case
    // historically.
    MOZ_ASSERT(Size() == OutputSize());
    pipe =
      SurfacePipeFactory::CreatePalettedSurfacePipe(this, mGIFStruct.images_decoded,
                                                    Size(), aFrameRect,
                                                    SurfaceFormat::B8G8R8A8,
                                                    aDepth, pipeFlags);
  }

  mCurrentFrameIndex = mGIFStruct.images_decoded;

  if (!pipe) {
    mPipe = SurfacePipe();
    return NS_ERROR_FAILURE;
  }

  mPipe = Move(*pipe);
  return NS_OK;
}


//******************************************************************************
void
nsGIFDecoder2::EndImageFrame()
{
  Opacity opacity = Opacity::SOME_TRANSPARENCY;

  if (mGIFStruct.images_decoded == 0) {
    // We need to send invalidations for the first frame.
    FlushImageData();

    // The first frame was preallocated with alpha; if it wasn't transparent, we
    // should fix that. We can also mark it opaque unconditionally if we didn't
    // actually see any transparent pixels - this test is only valid for the
    // first frame.
    if (!mGIFStruct.is_transparent && !mSawTransparency) {
      opacity = Opacity::FULLY_OPAQUE;
    }
  }

  // Unconditionally increment images_decoded, because we unconditionally
  // append frames in BeginImageFrame(). This ensures that images_decoded
  // always refers to the frame in mImage we're currently decoding,
  // even if some of them weren't decoded properly and thus are blank.
  mGIFStruct.images_decoded++;

  // Tell the superclass we finished a frame
  PostFrameStop(opacity,
                DisposalMethod(mGIFStruct.disposal_method),
                FrameTimeout::FromRawMilliseconds(mGIFStruct.delay_time));

  // Reset the transparent pixel
  if (mOldColor) {
    mColormap[mGIFStruct.tpixel] = mOldColor;
    mOldColor = 0;
  }

  mCurrentFrameIndex = -1;
}

template <typename PixelSize>
PixelSize
nsGIFDecoder2::ColormapIndexToPixel(uint8_t aIndex)
{
  MOZ_ASSERT(sizeof(PixelSize) == sizeof(uint32_t));

  // Retrieve the next color, clamping to the size of the colormap.
  uint32_t color = mColormap[aIndex & mColorMask];

  // Check for transparency.
  if (mGIFStruct.is_transparent) {
    mSawTransparency = mSawTransparency || color == 0;
  }

  return color;
}

template <>
uint8_t
nsGIFDecoder2::ColormapIndexToPixel<uint8_t>(uint8_t aIndex)
{
  return aIndex & mColorMask;
}

template <typename PixelSize>
NextPixel<PixelSize>
nsGIFDecoder2::YieldPixel(const uint8_t* aData,
                          size_t aLength,
                          size_t* aBytesReadOut)
{
  MOZ_ASSERT(aData);
  MOZ_ASSERT(aBytesReadOut);
  MOZ_ASSERT(mGIFStruct.stackp >= mGIFStruct.stack);

  // Advance to the next byte we should read.
  const uint8_t* data = aData + *aBytesReadOut;

  // If we don't have any decoded data to yield, try to read some input and
  // produce some.
  if (mGIFStruct.stackp == mGIFStruct.stack) {
    while (mGIFStruct.bits < mGIFStruct.codesize && *aBytesReadOut < aLength) {
      // Feed the next byte into the decoder's 32-bit input buffer.
      mGIFStruct.datum += int32_t(*data) << mGIFStruct.bits;
      mGIFStruct.bits += 8;
      data += 1;
      *aBytesReadOut += 1;
    }

    if (mGIFStruct.bits < mGIFStruct.codesize) {
      return AsVariant(WriteState::NEED_MORE_DATA);
    }

    // Get the leading variable-length symbol from the data stream.
    int code = mGIFStruct.datum & mGIFStruct.codemask;
    mGIFStruct.datum >>= mGIFStruct.codesize;
    mGIFStruct.bits -= mGIFStruct.codesize;

    const int clearCode = ClearCode();

    // Reset the dictionary to its original state, if requested
    if (code == clearCode) {
      mGIFStruct.codesize = mGIFStruct.datasize + 1;
      mGIFStruct.codemask = (1 << mGIFStruct.codesize) - 1;
      mGIFStruct.avail = clearCode + 2;
      mGIFStruct.oldcode = -1;
      return AsVariant(WriteState::NEED_MORE_DATA);
    }

    // Check for explicit end-of-stream code. It should only appear after all
    // image data, but if that was the case we wouldn't be in this function, so
    // this is always an error condition.
    if (code == (clearCode + 1)) {
      return AsVariant(WriteState::FAILURE);
    }

    if (mGIFStruct.oldcode == -1) {
      if (code >= MAX_BITS) {
        return AsVariant(WriteState::FAILURE);  // The code's too big; something's wrong.
      }

      mGIFStruct.firstchar = mGIFStruct.oldcode = code;

      // Yield a pixel at the appropriate index in the colormap.
      mGIFStruct.pixels_remaining--;
      return AsVariant(ColormapIndexToPixel<PixelSize>(mGIFStruct.suffix[code]));
    }

    int incode = code;
    if (code >= mGIFStruct.avail) {
      *mGIFStruct.stackp++ = mGIFStruct.firstchar;
      code = mGIFStruct.oldcode;

      if (mGIFStruct.stackp >= mGIFStruct.stack + MAX_BITS) {
        return AsVariant(WriteState::FAILURE);  // Stack overflow; something's wrong.
      }
    }

    while (code >= clearCode) {
      if ((code >= MAX_BITS) || (code == mGIFStruct.prefix[code])) {
        return AsVariant(WriteState::FAILURE);
      }

      *mGIFStruct.stackp++ = mGIFStruct.suffix[code];
      code = mGIFStruct.prefix[code];

      if (mGIFStruct.stackp >= mGIFStruct.stack + MAX_BITS) {
        return AsVariant(WriteState::FAILURE);  // Stack overflow; something's wrong.
      }
    }

    *mGIFStruct.stackp++ = mGIFStruct.firstchar = mGIFStruct.suffix[code];

    // Define a new codeword in the dictionary.
    if (mGIFStruct.avail < 4096) {
      mGIFStruct.prefix[mGIFStruct.avail] = mGIFStruct.oldcode;
      mGIFStruct.suffix[mGIFStruct.avail] = mGIFStruct.firstchar;
      mGIFStruct.avail++;

      // If we've used up all the codewords of a given length increase the
      // length of codewords by one bit, but don't exceed the specified maximum
      // codeword size of 12 bits.
      if (((mGIFStruct.avail & mGIFStruct.codemask) == 0) &&
          (mGIFStruct.avail < 4096)) {
        mGIFStruct.codesize++;
        mGIFStruct.codemask += mGIFStruct.avail;
      }
    }

    mGIFStruct.oldcode = incode;
  }

  if (MOZ_UNLIKELY(mGIFStruct.stackp <= mGIFStruct.stack)) {
    MOZ_ASSERT_UNREACHABLE("No decoded data but we didn't return early?");
    return AsVariant(WriteState::FAILURE);
  }

  // Yield a pixel at the appropriate index in the colormap.
  mGIFStruct.pixels_remaining--;
  return AsVariant(ColormapIndexToPixel<PixelSize>(*--mGIFStruct.stackp));
}

/// Expand the colormap from RGB to Packed ARGB as needed by Cairo.
/// And apply any LCMS transformation.
static void
ConvertColormap(uint32_t* aColormap, uint32_t aColors)
{
  // Apply CMS transformation if enabled and available
  if (gfxPlatform::GetCMSMode() == eCMSMode_All) {
    qcms_transform* transform = gfxPlatform::GetCMSRGBTransform();
    if (transform) {
      qcms_transform_data(transform, aColormap, aColormap, aColors);
    }
  }

  // Convert from the GIF's RGB format to the Cairo format.
  // Work from end to begin, because of the in-place expansion
  uint8_t* from = ((uint8_t*)aColormap) + 3 * aColors;
  uint32_t* to = aColormap + aColors;

  // Convert color entries to Cairo format

  // set up for loops below
  if (!aColors) {
    return;
  }
  uint32_t c = aColors;

  // copy as bytes until source pointer is 32-bit-aligned
  // NB: can't use 32-bit reads, they might read off the end of the buffer
  for (; (NS_PTR_TO_UINT32(from) & 0x3) && c; --c) {
    from -= 3;
    *--to = gfxPackedPixel(0xFF, from[0], from[1], from[2]);
  }

  // bulk copy of pixels.
  while (c >= 4) {
    from -= 12;
    to   -=  4;
    c    -=  4;
    GFX_BLOCK_RGB_TO_FRGB(from,to);
  }

  // copy remaining pixel(s)
  // NB: can't use 32-bit reads, they might read off the end of the buffer
  while (c--) {
    from -= 3;
    *--to = gfxPackedPixel(0xFF, from[0], from[1], from[2]);
  }
}

LexerResult
nsGIFDecoder2::DoDecode(SourceBufferIterator& aIterator, IResumable* aOnResume)
{
  MOZ_ASSERT(!HasError(), "Shouldn't call DoDecode after error!");

  return mLexer.Lex(aIterator, aOnResume,
                    [=](State aState, const char* aData, size_t aLength) {
    switch(aState) {
      case State::GIF_HEADER:
        return ReadGIFHeader(aData);
      case State::SCREEN_DESCRIPTOR:
        return ReadScreenDescriptor(aData);
      case State::GLOBAL_COLOR_TABLE:
        return ReadGlobalColorTable(aData, aLength);
      case State::FINISHED_GLOBAL_COLOR_TABLE:
        return FinishedGlobalColorTable();
      case State::BLOCK_HEADER:
        return ReadBlockHeader(aData);
      case State::EXTENSION_HEADER:
        return ReadExtensionHeader(aData);
      case State::GRAPHIC_CONTROL_EXTENSION:
        return ReadGraphicControlExtension(aData);
      case State::APPLICATION_IDENTIFIER:
        return ReadApplicationIdentifier(aData);
      case State::NETSCAPE_EXTENSION_SUB_BLOCK:
        return ReadNetscapeExtensionSubBlock(aData);
      case State::NETSCAPE_EXTENSION_DATA:
        return ReadNetscapeExtensionData(aData);
      case State::IMAGE_DESCRIPTOR:
        return ReadImageDescriptor(aData);
      case State::FINISH_IMAGE_DESCRIPTOR:
        return FinishImageDescriptor(aData);
      case State::LOCAL_COLOR_TABLE:
        return ReadLocalColorTable(aData, aLength);
      case State::FINISHED_LOCAL_COLOR_TABLE:
        return FinishedLocalColorTable();
      case State::IMAGE_DATA_BLOCK:
        return ReadImageDataBlock(aData);
      case State::IMAGE_DATA_SUB_BLOCK:
        return ReadImageDataSubBlock(aData);
      case State::LZW_DATA:
        return ReadLZWData(aData, aLength);
      case State::SKIP_LZW_DATA:
        return Transition::ContinueUnbuffered(State::SKIP_LZW_DATA);
      case State::FINISHED_LZW_DATA:
        return Transition::To(State::IMAGE_DATA_SUB_BLOCK, SUB_BLOCK_HEADER_LEN);
      case State::SKIP_SUB_BLOCKS:
        return SkipSubBlocks(aData);
      case State::SKIP_DATA_THEN_SKIP_SUB_BLOCKS:
        return Transition::ContinueUnbuffered(State::SKIP_DATA_THEN_SKIP_SUB_BLOCKS);
      case State::FINISHED_SKIPPING_DATA:
        return Transition::To(State::SKIP_SUB_BLOCKS, SUB_BLOCK_HEADER_LEN);
      default:
        MOZ_CRASH("Unknown State");
    }
  });
}

LexerTransition<nsGIFDecoder2::State>
nsGIFDecoder2::ReadGIFHeader(const char* aData)
{
  // We retrieve the version here but because many GIF encoders set header
  // fields incorrectly, we barely use it; features which should only appear in
  // GIF89a are always accepted.
  if (strncmp(aData, "GIF87a", GIF_HEADER_LEN) == 0) {
    mGIFStruct.version = 87;
  } else if (strncmp(aData, "GIF89a", GIF_HEADER_LEN) == 0) {
    mGIFStruct.version = 89;
  } else {
    return Transition::TerminateFailure();
  }

  return Transition::To(State::SCREEN_DESCRIPTOR, GIF_SCREEN_DESCRIPTOR_LEN);
}

LexerTransition<nsGIFDecoder2::State>
nsGIFDecoder2::ReadScreenDescriptor(const char* aData)
{
  mGIFStruct.screen_width  = LittleEndian::readUint16(aData + 0);
  mGIFStruct.screen_height = LittleEndian::readUint16(aData + 2);

  const uint8_t packedFields = aData[4];

  // XXX: Should we be capturing these values even if there is no global color
  // table?
  mGIFStruct.global_colormap_depth =
    (packedFields & PACKED_FIELDS_TABLE_DEPTH_MASK) + 1;
  mGIFStruct.global_colormap_count = 1 << mGIFStruct.global_colormap_depth;

  // We ignore several fields in the header. We don't care about the 'sort
  // flag', which indicates if the global color table's entries are sorted in
  // order of importance - if we need to render this image for a device with a
  // narrower color gamut than GIF supports we'll handle that at a different
  // layer. We have no use for the pixel aspect ratio as well. Finally, we
  // intentionally ignore the background color index, as implementing that
  // feature would not be web compatible - when a GIF image frame doesn't cover
  // the entire area of the image, the area that's not covered should always be
  // transparent.

  if (packedFields & PACKED_FIELDS_COLOR_TABLE_BIT) {
    MOZ_ASSERT(mColorTablePos == 0);

    // We read the global color table in unbuffered mode since it can be quite
    // large and it'd be preferable to avoid unnecessary copies.
    const size_t globalColorTableSize = 3 * mGIFStruct.global_colormap_count;
    return Transition::ToUnbuffered(State::FINISHED_GLOBAL_COLOR_TABLE,
                                    State::GLOBAL_COLOR_TABLE,
                                    globalColorTableSize);
  }

  return Transition::To(State::BLOCK_HEADER, BLOCK_HEADER_LEN);
}

LexerTransition<nsGIFDecoder2::State>
nsGIFDecoder2::ReadGlobalColorTable(const char* aData, size_t aLength)
{
  uint8_t* dest = reinterpret_cast<uint8_t*>(mGIFStruct.global_colormap)
                + mColorTablePos;
  memcpy(dest, aData, aLength);
  mColorTablePos += aLength;
  return Transition::ContinueUnbuffered(State::GLOBAL_COLOR_TABLE);
}

LexerTransition<nsGIFDecoder2::State>
nsGIFDecoder2::FinishedGlobalColorTable()
{
  ConvertColormap(mGIFStruct.global_colormap, mGIFStruct.global_colormap_count);
  mColorTablePos = 0;
  return Transition::To(State::BLOCK_HEADER, BLOCK_HEADER_LEN);
}

LexerTransition<nsGIFDecoder2::State>
nsGIFDecoder2::ReadBlockHeader(const char* aData)
{
  // Determine what type of block we're dealing with.
  switch (aData[0]) {
    case GIF_EXTENSION_INTRODUCER:
      return Transition::To(State::EXTENSION_HEADER, EXTENSION_HEADER_LEN);

    case GIF_IMAGE_SEPARATOR:
      return Transition::To(State::IMAGE_DESCRIPTOR, IMAGE_DESCRIPTOR_LEN);

    case GIF_TRAILER:
      FinishInternal();
      return Transition::TerminateSuccess();

    default:
      // If we get anything other than GIF_IMAGE_SEPARATOR,
      // GIF_EXTENSION_INTRODUCER, or GIF_TRAILER, there is extraneous data
      // between blocks. The GIF87a spec tells us to keep reading until we find
      // an image separator, but GIF89a says such a file is corrupt. We follow
      // GIF89a and bail out.

      if (mGIFStruct.images_decoded > 0) {
        // The file is corrupt, but we successfully decoded some frames, so we
        // may as well consider the decode successful and display them.
        FinishInternal();
        return Transition::TerminateSuccess();
      }

      // No images decoded; there is nothing to display.
      return Transition::TerminateFailure();
  }
}

LexerTransition<nsGIFDecoder2::State>
nsGIFDecoder2::ReadExtensionHeader(const char* aData)
{
  const uint8_t label = aData[0];
  const uint8_t extensionHeaderLength = aData[1];

  // If the extension header is zero length, just treat it as a block terminator
  // and move on to the next block immediately.
  if (extensionHeaderLength == 0) {
    return Transition::To(State::BLOCK_HEADER, BLOCK_HEADER_LEN);
  }

  switch (label) {
    case GIF_GRAPHIC_CONTROL_LABEL:
      // The GIF spec mandates that the Control Extension header block length is
      // 4 bytes, and the parser for this block reads 4 bytes, so we must
      // enforce that the buffer contains at least this many bytes. If the GIF
      // specifies a different length, we allow that, so long as it's larger;
      // the additional data will simply be ignored.
      return Transition::To(State::GRAPHIC_CONTROL_EXTENSION,
                            max<uint8_t>(extensionHeaderLength,
                                         GRAPHIC_CONTROL_EXTENSION_LEN));

    case GIF_APPLICATION_EXTENSION_LABEL:
      // Again, the spec specifies that an application extension header is 11
      // bytes, but for compatibility with GIFs in the wild, we allow deviation
      // from the spec. This is important for real-world compatibility, as GIFs
      // in the wild exist with application extension headers that are both
      // shorter and longer than 11 bytes. However, we only try to actually
      // interpret the application extension if the length is correct;
      // otherwise, we just skip the block unconditionally.
      return extensionHeaderLength == APPLICATION_EXTENSION_LEN
           ? Transition::To(State::APPLICATION_IDENTIFIER, extensionHeaderLength)
           : Transition::ToUnbuffered(State::FINISHED_SKIPPING_DATA,
                                      State::SKIP_DATA_THEN_SKIP_SUB_BLOCKS,
                                      extensionHeaderLength);

    default:
      // Skip over any other type of extension block, including comment and
      // plain text blocks.
      return Transition::ToUnbuffered(State::FINISHED_SKIPPING_DATA,
                                      State::SKIP_DATA_THEN_SKIP_SUB_BLOCKS,
                                      extensionHeaderLength);
  }
}

LexerTransition<nsGIFDecoder2::State>
nsGIFDecoder2::ReadGraphicControlExtension(const char* aData)
{
  mGIFStruct.is_transparent = aData[0] & 0x1;
  mGIFStruct.tpixel = uint8_t(aData[3]);
  mGIFStruct.disposal_method = (aData[0] >> 2) & 0x7;

  if (mGIFStruct.disposal_method == 4) {
    // Some encoders (and apparently some specs) represent
    // DisposalMethod::RESTORE_PREVIOUS as 4, but 3 is used in the canonical
    // spec and is more popular, so we normalize to 3.
    mGIFStruct.disposal_method = 3;
  } else if (mGIFStruct.disposal_method > 4) {
    // This GIF is using a disposal method which is undefined in the spec.
    // Treat it as DisposalMethod::NOT_SPECIFIED.
    mGIFStruct.disposal_method = 0;
  }

  DisposalMethod method = DisposalMethod(mGIFStruct.disposal_method);
  if (method == DisposalMethod::CLEAR_ALL || method == DisposalMethod::CLEAR) {
    // We may have to display the background under this image during animation
    // playback, so we regard it as transparent.
    PostHasTransparency();
  }

  mGIFStruct.delay_time = LittleEndian::readUint16(aData + 1) * 10;
  if (mGIFStruct.delay_time > 0) {
    PostIsAnimated(FrameTimeout::FromRawMilliseconds(mGIFStruct.delay_time));
  }

  return Transition::To(State::SKIP_SUB_BLOCKS, SUB_BLOCK_HEADER_LEN);
}

LexerTransition<nsGIFDecoder2::State>
nsGIFDecoder2::ReadApplicationIdentifier(const char* aData)
{
  if ((strncmp(aData, "NETSCAPE2.0", 11) == 0) ||
      (strncmp(aData, "ANIMEXTS1.0", 11) == 0)) {
    // This is a Netscape application extension block.
    return Transition::To(State::NETSCAPE_EXTENSION_SUB_BLOCK,
                          SUB_BLOCK_HEADER_LEN);
  }

  // This is an application extension we don't care about. Just skip it.
  return Transition::To(State::SKIP_SUB_BLOCKS, SUB_BLOCK_HEADER_LEN);
}

LexerTransition<nsGIFDecoder2::State>
nsGIFDecoder2::ReadNetscapeExtensionSubBlock(const char* aData)
{
  const uint8_t blockLength = aData[0];
  if (blockLength == 0) {
    // We hit the block terminator.
    return Transition::To(State::BLOCK_HEADER, BLOCK_HEADER_LEN);
  }

  // We consume a minimum of 3 bytes in accordance with the specs for the
  // Netscape application extension block, such as they are.
  const size_t extensionLength = max<uint8_t>(blockLength, 3);
  return Transition::To(State::NETSCAPE_EXTENSION_DATA, extensionLength);
}

LexerTransition<nsGIFDecoder2::State>
nsGIFDecoder2::ReadNetscapeExtensionData(const char* aData)
{
  // Documentation for NETSCAPE2.0 / ANIMEXTS1.0 extensions can be found at:
  //   https://wiki.whatwg.org/wiki/GIF
  static const uint8_t NETSCAPE_LOOPING_EXTENSION_SUB_BLOCK_ID = 1;
  static const uint8_t NETSCAPE_BUFFERING_EXTENSION_SUB_BLOCK_ID = 2;

  const uint8_t subBlockID = aData[0] & 7;
  switch (subBlockID) {
    case NETSCAPE_LOOPING_EXTENSION_SUB_BLOCK_ID:
      // This is looping extension.
      mGIFStruct.loop_count = LittleEndian::readUint16(aData + 1);
      return Transition::To(State::NETSCAPE_EXTENSION_SUB_BLOCK,
                            SUB_BLOCK_HEADER_LEN);

    case NETSCAPE_BUFFERING_EXTENSION_SUB_BLOCK_ID:
      // We allow, but ignore, this extension.
      return Transition::To(State::NETSCAPE_EXTENSION_SUB_BLOCK,
                            SUB_BLOCK_HEADER_LEN);

    default:
      return Transition::TerminateFailure();
  }
}

LexerTransition<nsGIFDecoder2::State>
nsGIFDecoder2::ReadImageDescriptor(const char* aData)
{
  // On the first frame, we don't need to yield, and none of the other checks
  // below apply, so we can just jump right into FinishImageDescriptor().
  if (mGIFStruct.images_decoded == 0) {
    return FinishImageDescriptor(aData);
  }

  if (!HasAnimation()) {
    // We should've already called PostIsAnimated(); this must be a corrupt
    // animated image with a first frame timeout of zero. Signal that we're
    // animated now, before the first-frame decode early exit below, so that
    // RasterImage can detect that this happened.
    PostIsAnimated(FrameTimeout::FromRawMilliseconds(0));
  }

  if (IsFirstFrameDecode()) {
    // We're about to get a second frame, but we only want the first. Stop
    // decoding now.
    FinishInternal();
    return Transition::TerminateSuccess();
  }

  MOZ_ASSERT(Size() == OutputSize(), "Downscaling an animated image?");

  // Yield to allow access to the previous frame before we start a new one.
  return Transition::ToAfterYield(State::FINISH_IMAGE_DESCRIPTOR);
}

LexerTransition<nsGIFDecoder2::State>
nsGIFDecoder2::FinishImageDescriptor(const char* aData)
{
  IntRect frameRect;

  // Get image offsets with respect to the screen origin.
  frameRect.x = LittleEndian::readUint16(aData + 0);
  frameRect.y = LittleEndian::readUint16(aData + 2);
  frameRect.width = LittleEndian::readUint16(aData + 4);
  frameRect.height = LittleEndian::readUint16(aData + 6);

  if (!mGIFStruct.images_decoded) {
    // Work around GIF files where
    //   * at least one of the logical screen dimensions is smaller than the
    //     same dimension in the first image, or
    //   * GIF87a files where the first image's dimensions do not match the
    //     logical screen dimensions.
    if (mGIFStruct.screen_height < frameRect.height ||
        mGIFStruct.screen_width < frameRect.width ||
        mGIFStruct.version == 87) {
      mGIFStruct.screen_height = frameRect.height;
      mGIFStruct.screen_width = frameRect.width;
      frameRect.MoveTo(0, 0);
    }

    // Create the image container with the right size.
    BeginGIF();
    if (HasError()) {
      // Setting the size led to an error.
      return Transition::TerminateFailure();
    }

    // If we're doing a metadata decode, we're done.
    if (IsMetadataDecode()) {
      CheckForTransparency(frameRect);
      FinishInternal();
      return Transition::TerminateSuccess();
    }
  }

  // Work around broken GIF files that have zero frame width or height; in this
  // case, we'll treat the frame as having the same size as the overall image.
  if (frameRect.height == 0 || frameRect.width == 0) {
    frameRect.height = mGIFStruct.screen_height;
    frameRect.width = mGIFStruct.screen_width;

    // If that still resulted in zero frame width or height, give up.
    if (frameRect.height == 0 || frameRect.width == 0) {
      return Transition::TerminateFailure();
    }
  }

  // Determine |depth| (log base 2 of the number of colors in the palette).
  bool haveLocalColorTable = false;
  uint16_t depth = 0;
  uint8_t packedFields = aData[8];

  if (packedFields & PACKED_FIELDS_COLOR_TABLE_BIT) {
    // Get the palette depth from the local color table.
    depth = (packedFields & PACKED_FIELDS_TABLE_DEPTH_MASK) + 1;
    haveLocalColorTable = true;
  } else {
    // Get the palette depth from the global color table.
    depth = mGIFStruct.global_colormap_depth;
  }

  // If the transparent color index is greater than the number of colors in the
  // color table, we may need a higher color depth than |depth| would specify.
  // Our internal representation of the image will instead use |realDepth|,
  // which is the smallest color depth that can accomodate the existing palette
  // *and* the transparent color index.
  uint16_t realDepth = depth;
  while (mGIFStruct.tpixel >= (1 << realDepth) &&
         realDepth < 8) {
    realDepth++;
  }

  // Create a mask used to ensure that color values fit within the colormap.
  mColorMask = 0xFF >> (8 - realDepth);

  // Determine if this frame is interlaced or not.
  const bool isInterlaced = packedFields & PACKED_FIELDS_INTERLACED_BIT;

  // Create the SurfacePipe we'll use to write output for this frame.
  if (NS_FAILED(BeginImageFrame(frameRect, realDepth, isInterlaced))) {
    return Transition::TerminateFailure();
  }

  // Clear state from last image.
  mGIFStruct.pixels_remaining = frameRect.width * frameRect.height;

  if (haveLocalColorTable) {
    // We have a local color table, so prepare to read it into the palette of
    // the current frame.
    mGIFStruct.local_colormap_size = 1 << depth;

    if (mGIFStruct.images_decoded == 0) {
      // The first frame has a local color table. Allocate space for it as we
      // use a BGRA or BGRX surface for the first frame; such surfaces don't
      // have their own palettes internally.
      mColormapSize = sizeof(uint32_t) << realDepth;
      if (!mGIFStruct.local_colormap) {
        mGIFStruct.local_colormap =
          static_cast<uint32_t*>(moz_xmalloc(mColormapSize));
      }
      mColormap = mGIFStruct.local_colormap;
    }

    const size_t size = 3 << depth;
    if (mColormapSize > size) {
      // Clear the part of the colormap which will be unused with this palette.
      // If a GIF references an invalid palette entry, ensure the entry is opaque white.
      // This is needed for Skia as if it isn't, RGBX surfaces will cause blending issues
      // with Skia.
      memset(reinterpret_cast<uint8_t*>(mColormap) + size, 0xFF,
             mColormapSize - size);
    }

    MOZ_ASSERT(mColorTablePos == 0);

    // We read the local color table in unbuffered mode since it can be quite
    // large and it'd be preferable to avoid unnecessary copies.
    return Transition::ToUnbuffered(State::FINISHED_LOCAL_COLOR_TABLE,
                                    State::LOCAL_COLOR_TABLE,
                                    size);
  }

  // There's no local color table; copy the global color table into the palette
  // of the current frame.
  if (mGIFStruct.images_decoded > 0) {
    memcpy(mColormap, mGIFStruct.global_colormap, mColormapSize);
  } else {
    mColormap = mGIFStruct.global_colormap;
  }

  return Transition::To(State::IMAGE_DATA_BLOCK, BLOCK_HEADER_LEN);
}

LexerTransition<nsGIFDecoder2::State>
nsGIFDecoder2::ReadLocalColorTable(const char* aData, size_t aLength)
{
  uint8_t* dest = reinterpret_cast<uint8_t*>(mColormap) + mColorTablePos;
  memcpy(dest, aData, aLength);
  mColorTablePos += aLength;
  return Transition::ContinueUnbuffered(State::LOCAL_COLOR_TABLE);
}

LexerTransition<nsGIFDecoder2::State>
nsGIFDecoder2::FinishedLocalColorTable()
{
  ConvertColormap(mColormap, mGIFStruct.local_colormap_size);
  mColorTablePos = 0;
  return Transition::To(State::IMAGE_DATA_BLOCK, BLOCK_HEADER_LEN);
}

LexerTransition<nsGIFDecoder2::State>
nsGIFDecoder2::ReadImageDataBlock(const char* aData)
{
  // Make sure the transparent pixel is transparent in the colormap.
  if (mGIFStruct.is_transparent) {
    // Save the old value so we can restore it later.
    if (mColormap == mGIFStruct.global_colormap) {
        mOldColor = mColormap[mGIFStruct.tpixel];
    }
    mColormap[mGIFStruct.tpixel] = 0;
  }

  // Initialize the LZW decoder.
  mGIFStruct.datasize = uint8_t(aData[0]);
  const int clearCode = ClearCode();
  if (mGIFStruct.datasize > MAX_LZW_BITS || clearCode >= MAX_BITS) {
    return Transition::TerminateFailure();
  }

  mGIFStruct.avail = clearCode + 2;
  mGIFStruct.oldcode = -1;
  mGIFStruct.codesize = mGIFStruct.datasize + 1;
  mGIFStruct.codemask = (1 << mGIFStruct.codesize) - 1;
  mGIFStruct.datum = mGIFStruct.bits = 0;

  // Initialize the tables.
  for (int i = 0; i < clearCode; i++) {
    mGIFStruct.suffix[i] = i;
  }

  mGIFStruct.stackp = mGIFStruct.stack;

  // Begin reading image data sub-blocks.
  return Transition::To(State::IMAGE_DATA_SUB_BLOCK, SUB_BLOCK_HEADER_LEN);
}

LexerTransition<nsGIFDecoder2::State>
nsGIFDecoder2::ReadImageDataSubBlock(const char* aData)
{
  const uint8_t subBlockLength = aData[0];
  if (subBlockLength == 0) {
    // We hit the block terminator.
    EndImageFrame();
    return Transition::To(State::BLOCK_HEADER, BLOCK_HEADER_LEN);
  }

  if (mGIFStruct.pixels_remaining == 0) {
    // We've already written to the entire image; we should've hit the block
    // terminator at this point. This image is corrupt, but we'll tolerate it.

    if (subBlockLength == GIF_TRAILER) {
      // This GIF is missing the block terminator for the final block; we'll put
      // up with it.
      FinishInternal();
      return Transition::TerminateSuccess();
    }

    // We're not at the end of the image, so just skip the extra data.
    return Transition::ToUnbuffered(State::FINISHED_LZW_DATA,
                                    State::SKIP_LZW_DATA,
                                    subBlockLength);
  }

  // Handle the standard case: there's data in the sub-block and pixels left to
  // fill in the image. We read the sub-block unbuffered so we can get pixels on
  // the screen as soon as possible.
  return Transition::ToUnbuffered(State::FINISHED_LZW_DATA,
                                  State::LZW_DATA,
                                  subBlockLength);
}

LexerTransition<nsGIFDecoder2::State>
nsGIFDecoder2::ReadLZWData(const char* aData, size_t aLength)
{
  const uint8_t* data = reinterpret_cast<const uint8_t*>(aData);
  size_t length = aLength;

  while (mGIFStruct.pixels_remaining > 0 &&
         (length > 0 || mGIFStruct.bits >= mGIFStruct.codesize)) {
    size_t bytesRead = 0;

    auto result = mGIFStruct.images_decoded == 0
      ? mPipe.WritePixels<uint32_t>([&]{ return YieldPixel<uint32_t>(data, length, &bytesRead); })
      : mPipe.WritePixels<uint8_t>([&]{ return YieldPixel<uint8_t>(data, length, &bytesRead); });

    if (MOZ_UNLIKELY(bytesRead > length)) {
      MOZ_ASSERT_UNREACHABLE("Overread?");
      bytesRead = length;
    }

    // Advance our position in the input based upon what YieldPixel() consumed.
    data += bytesRead;
    length -= bytesRead;

    switch (result) {
      case WriteState::NEED_MORE_DATA:
        continue;

      case WriteState::FINISHED:
        NS_WARNING_ASSERTION(mGIFStruct.pixels_remaining <= 0,
                             "too many pixels");
        mGIFStruct.pixels_remaining = 0;
        break;

      case WriteState::FAILURE:
        return Transition::TerminateFailure();
    }
  }

  // We're done, but keep going until we consume all the data in the sub-block.
  return Transition::ContinueUnbuffered(State::LZW_DATA);
}

LexerTransition<nsGIFDecoder2::State>
nsGIFDecoder2::SkipSubBlocks(const char* aData)
{
  // In the SKIP_SUB_BLOCKS state we skip over data sub-blocks that we're not
  // interested in. Blocks consist of a block header (which can be up to 255
  // bytes in length) and a series of data sub-blocks. Each data sub-block
  // consists of a single byte length value, followed by the data itself. A data
  // sub-block with a length of zero terminates the overall block.
  // SKIP_SUB_BLOCKS reads a sub-block length value. If it's zero, we've arrived
  // at the next block. Otherwise, we enter the SKIP_DATA_THEN_SKIP_SUB_BLOCKS
  // state to skip over the sub-block data and return to SKIP_SUB_BLOCKS at the
  // start of the next sub-block.

  const uint8_t nextSubBlockLength = aData[0];
  if (nextSubBlockLength == 0) {
    // We hit the block terminator, so the sequence of data sub-blocks is over;
    // begin processing another block.
    return Transition::To(State::BLOCK_HEADER, BLOCK_HEADER_LEN);
  }

  // Skip to the next sub-block length value.
  return Transition::ToUnbuffered(State::FINISHED_SKIPPING_DATA,
                                  State::SKIP_DATA_THEN_SKIP_SUB_BLOCKS,
                                  nextSubBlockLength);
}

Maybe<Telemetry::ID>
nsGIFDecoder2::SpeedHistogram() const
{
  return Some(Telemetry::IMAGE_DECODE_SPEED_GIF);
}

} // namespace image
} // namespace mozilla
