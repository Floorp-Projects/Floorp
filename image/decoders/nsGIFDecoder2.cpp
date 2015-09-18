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

#include <stddef.h>

#include "nsGIFDecoder2.h"
#include "nsIInputStream.h"
#include "RasterImage.h"

#include "gfxColor.h"
#include "gfxPlatform.h"
#include "qcms.h"
#include <algorithm>
#include "mozilla/Telemetry.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace image {

// GETN(n, s) requests at least 'n' bytes available from 'q', at start of state
// 's'. Colormaps are directly copied in the resp. global_colormap or the
// local_colormap of the PAL image frame So a fixed buffer in gif_struct is
// good enough. This buffer is only needed to copy left-over data from one
// GifWrite call to the next
#define GETN(n,s)                      \
  PR_BEGIN_MACRO                       \
    mGIFStruct.bytes_to_consume = (n); \
    mGIFStruct.state = (s);            \
  PR_END_MACRO

// Get a 16-bit value stored in little-endian format
#define GETINT16(p)   ((p)[1]<<8|(p)[0])
//////////////////////////////////////////////////////////////////////
// GIF Decoder Implementation

nsGIFDecoder2::nsGIFDecoder2(RasterImage* aImage)
  : Decoder(aImage)
  , mCurrentRow(-1)
  , mLastFlushedRow(-1)
  , mOldColor(0)
  , mCurrentFrameIndex(-1)
  , mCurrentPass(0)
  , mLastFlushedPass(0)
  , mGIFOpen(false)
  , mSawTransparency(false)
{
  // Clear out the structure, excluding the arrays
  memset(&mGIFStruct, 0, sizeof(mGIFStruct));

  // Initialize as "animate once" in case no NETSCAPE2.0 extension is found
  mGIFStruct.loop_count = 1;

  // Start with the version (GIF89a|GIF87a)
  mGIFStruct.state = gif_type;
  mGIFStruct.bytes_to_consume = 6;
}

nsGIFDecoder2::~nsGIFDecoder2()
{
  free(mGIFStruct.local_colormap);
  free(mGIFStruct.hold);
}

nsresult
nsGIFDecoder2::SetTargetSize(const nsIntSize& aSize)
{
  // Make sure the size is reasonable.
  if (MOZ_UNLIKELY(aSize.width <= 0 || aSize.height <= 0)) {
    return NS_ERROR_FAILURE;
  }

  // Create a downscaler that we'll filter our output through.
  mDownscaler.emplace(aSize);

  return NS_OK;
}

uint8_t*
nsGIFDecoder2::GetCurrentRowBuffer()
{
  if (!mDownscaler) {
    MOZ_ASSERT(!mDeinterlacer, "Deinterlacer without downscaler?");
    uint32_t bpp = mGIFStruct.images_decoded == 0 ? sizeof(uint32_t)
                                                  : sizeof(uint8_t);
    return mImageData + mGIFStruct.irow * mGIFStruct.width * bpp;
  }

  if (!mDeinterlacer) {
    return mDownscaler->RowBuffer();
  }

  return mDeinterlacer->RowBuffer(mGIFStruct.irow);
}

uint8_t*
nsGIFDecoder2::GetRowBuffer(uint32_t aRow)
{
  MOZ_ASSERT(mGIFStruct.images_decoded == 0,
             "Calling GetRowBuffer on a frame other than the first suggests "
             "we're deinterlacing animated frames");
  MOZ_ASSERT(!mDownscaler || mDeinterlacer,
             "Can't get buffer for a specific row if downscaling "
             "but not deinterlacing");

  if (mDownscaler) {
    return mDeinterlacer->RowBuffer(aRow);
  }

  return mImageData + aRow * mGIFStruct.width * sizeof(uint32_t);
}

void
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
}

// Push any new rows according to mCurrentPass/mLastFlushedPass and
// mCurrentRow/mLastFlushedRow.  Note: caller is responsible for
// updating mlastFlushed{Row,Pass}.
void
nsGIFDecoder2::FlushImageData(uint32_t fromRow, uint32_t rows)
{
  nsIntRect r(mGIFStruct.x_offset, mGIFStruct.y_offset + fromRow,
              mGIFStruct.width, rows);
  PostInvalidation(r);
}

void
nsGIFDecoder2::FlushImageData()
{
  if (mDownscaler) {
    if (mDownscaler->HasInvalidation()) {
      DownscalerInvalidRect invalidRect = mDownscaler->TakeInvalidRect();
      PostInvalidation(invalidRect.mOriginalSizeRect,
                       Some(invalidRect.mTargetSizeRect));
    }
    return;
  }

  switch (mCurrentPass - mLastFlushedPass) {
    case 0:  // same pass
      if (mCurrentRow - mLastFlushedRow) {
        FlushImageData(mLastFlushedRow + 1, mCurrentRow - mLastFlushedRow);
      }
      break;

    case 1:  // one pass on - need to handle bottom & top rects
      FlushImageData(0, mCurrentRow + 1);
      FlushImageData(mLastFlushedRow + 1,
                     mGIFStruct.height - (mLastFlushedRow + 1));
      break;

    default: // more than one pass on - push the whole frame
      FlushImageData(0, mGIFStruct.height);
  }
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

void
nsGIFDecoder2::CheckForTransparency(IntRect aFrameRect)
{
  // Check if the image has a transparent color in its palette.
  if (mGIFStruct.is_transparent) {
    PostHasTransparency();
    return;
  }

  if (mGIFStruct.images_decoded > 0) {
    return;  // We only care about first frame padding below.
  }

  // If we need padding on the first frame, that means we don't draw into part
  // of the image at all. Report that as transparency.
  IntRect imageRect(0, 0, mGIFStruct.screen_width, mGIFStruct.screen_height);
  if (!imageRect.IsEqualEdges(aFrameRect)) {
    PostHasTransparency();
  }
}

//******************************************************************************
nsresult
nsGIFDecoder2::BeginImageFrame(uint16_t aDepth)
{
  MOZ_ASSERT(HasSize());

  gfx::SurfaceFormat format;
  if (mGIFStruct.is_transparent) {
    format = gfx::SurfaceFormat::B8G8R8A8;
  } else {
    format = gfx::SurfaceFormat::B8G8R8X8;
  }

  IntRect frameRect(mGIFStruct.x_offset, mGIFStruct.y_offset,
                    mGIFStruct.width, mGIFStruct.height);

  CheckForTransparency(frameRect);

  // Make sure there's no animation if we're downscaling.
  MOZ_ASSERT_IF(mDownscaler, !GetImageMetadata().HasAnimation());

  IntSize targetSize = mDownscaler ? mDownscaler->TargetSize()
                                   : GetSize();

  // Rescale the frame rect for the target size.
  IntRect targetFrameRect = frameRect;
  if (mDownscaler) {
    IntSize originalSize = GetSize();
    targetFrameRect.ScaleRoundOut(double(targetSize.width) / originalSize.width,
                                  double(targetSize.height) / originalSize.height);
  }

  // Use correct format, RGB for first frame, PAL for following frames
  // and include transparency to allow for optimization of opaque images
  nsresult rv = NS_OK;
  if (mGIFStruct.images_decoded) {
    // Image data is stored with original depth and palette.
    rv = AllocateFrame(mGIFStruct.images_decoded, targetSize,
                       targetFrameRect, format, aDepth);
  } else {
    // Regardless of depth of input, the first frame is decoded into 24bit RGB.
    rv = AllocateFrame(mGIFStruct.images_decoded, targetSize,
                       targetFrameRect, format);
  }

  mCurrentFrameIndex = mGIFStruct.images_decoded;

  if (NS_FAILED(rv)) {
    return rv;
  }

  if (mDownscaler) {
    rv = mDownscaler->BeginFrame(frameRect.Size(), mImageData,
                                 mGIFStruct.is_transparent);
  }

  return rv;
}


//******************************************************************************
void
nsGIFDecoder2::EndImageFrame()
{
  Opacity opacity = Opacity::SOME_TRANSPARENCY;

  // First flush all pending image data
  if (!mGIFStruct.images_decoded) {
    // Only need to flush first frame
    FlushImageData();

    // If the first frame is smaller in height than the entire image, send an
    // invalidation for the area it does not have data for.
    // This will clear the remaining bits of the placeholder. (Bug 37589)
    const uint32_t realFrameHeight = mGIFStruct.height + mGIFStruct.y_offset;
    if (realFrameHeight < mGIFStruct.screen_height) {
      if (mDownscaler) {
        IntRect targetRect = IntRect(IntPoint(), mDownscaler->TargetSize());
        PostInvalidation(IntRect(IntPoint(), GetSize()), Some(targetRect));
      } else {
        nsIntRect r(0, realFrameHeight,
                    mGIFStruct.screen_width,
                    mGIFStruct.screen_height - realFrameHeight);
        PostInvalidation(r);
      }
    }

    // The first frame was preallocated with alpha; if it wasn't transparent, we
    // should fix that. We can also mark it opaque unconditionally if we didn't
    // actually see any transparent pixels - this test is only valid for the
    // first frame.
    if (!mGIFStruct.is_transparent || !mSawTransparency) {
      opacity = Opacity::OPAQUE;
    }
  }
  mCurrentRow = mLastFlushedRow = -1;
  mCurrentPass = mLastFlushedPass = 0;

  // Only add frame if we have any rows at all
  if (mGIFStruct.rows_remaining != mGIFStruct.height) {
    if (mGIFStruct.rows_remaining && mGIFStruct.images_decoded) {
      // Clear the remaining rows (only needed for the animation frames)
      uint8_t* rowp =
        mImageData + ((mGIFStruct.height - mGIFStruct.rows_remaining) *
                      mGIFStruct.width);
      memset(rowp, 0, mGIFStruct.rows_remaining * mGIFStruct.width);
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
                mGIFStruct.delay_time);

  // Reset the transparent pixel
  if (mOldColor) {
    mColormap[mGIFStruct.tpixel] = mOldColor;
    mOldColor = 0;
  }

  mCurrentFrameIndex = -1;
}


//******************************************************************************
// Send the data to the display front-end.
uint32_t
nsGIFDecoder2::OutputRow()
{
  // Initialize the region in which we're duplicating rows (for the
  // Haeberli-inspired hack below) to |irow|, which is the row we're writing to
  // now.
  int drow_start = mGIFStruct.irow;
  int drow_end = mGIFStruct.irow;

  // Protect against too much image data
  if ((unsigned)drow_start >= mGIFStruct.height) {
    NS_WARNING("GIF2.cpp::OutputRow - too much image data");
    return 0;
  }

  if (!mGIFStruct.images_decoded) {
    // Haeberli-inspired hack for interlaced GIFs: Replicate lines while
    // displaying to diminish the "venetian-blind" effect as the image is
    // loaded. Adjust pixel vertical positions to avoid the appearance of the
    // image crawling up the screen as successive passes are drawn.
    if (mGIFStruct.progressive_display && mGIFStruct.interlaced &&
        (mGIFStruct.ipass < 4)) {
      // ipass = 1,2,3 results in resp. row_dup = 7,3,1 and row_shift = 3,1,0
      const uint32_t row_dup = 15 >> mGIFStruct.ipass;
      const uint32_t row_shift = row_dup >> 1;

      drow_start -= row_shift;
      drow_end = drow_start + row_dup;

      // Extend if bottom edge isn't covered because of the shift upward.
      if (((mGIFStruct.height - 1) - drow_end) <= row_shift) {
        drow_end = mGIFStruct.height - 1;
      }

      // Clamp first and last rows to upper and lower edge of image.
      if (drow_start < 0) {
        drow_start = 0;
      }
      if ((unsigned)drow_end >= mGIFStruct.height) {
        drow_end = mGIFStruct.height - 1;
      }
    }

    // Row to process
    uint8_t* rowp = GetCurrentRowBuffer();

    // Convert color indices to Cairo pixels
    uint8_t* from = rowp + mGIFStruct.width;
    uint32_t* to = ((uint32_t*)rowp) + mGIFStruct.width;
    uint32_t* cmap = mColormap;
    for (uint32_t c = mGIFStruct.width; c > 0; c--) {
      *--to = cmap[*--from];
    }

    // check for alpha (only for first frame)
    if (mGIFStruct.is_transparent && !mSawTransparency) {
      const uint32_t* rgb = (uint32_t*)rowp;
      for (uint32_t i = mGIFStruct.width; i > 0; i--) {
        if (*rgb++ == 0) {
          mSawTransparency = true;
          break;
        }
      }
    }

    // If we're downscaling but not deinterlacing, we're done with this row and
    // can commit it now. Otherwise, we'll let Deinterlacer do the committing
    // when we call PropagatePassToDownscaler() at the end of this pass.
    if (mDownscaler && !mDeinterlacer) {
      mDownscaler->CommitRow();
    }

    if (drow_end > drow_start) {
      // Duplicate rows if needed to reduce the "venetian blind" effect mentioned
      // above. This writes out scanlines of the image in a way that isn't ordered
      // vertically, which is incompatible with the filter that we use for
      // downscale-during-decode, so we can't do this if we're downscaling.
      MOZ_ASSERT_IF(mDownscaler, mDeinterlacer);
      const uint32_t bpr = sizeof(uint32_t) * mGIFStruct.width;
      for (int r = drow_start; r <= drow_end; r++) {
        // Skip the row we wrote to above; that's what we're copying *from*.
        if (r != int(mGIFStruct.irow)) {
          memcpy(GetRowBuffer(r), rowp, bpr);
        }
      }
    }
  }

  mCurrentRow = drow_end;
  mCurrentPass = mGIFStruct.ipass;
  if (mGIFStruct.ipass == 1) {
    mLastFlushedPass = mGIFStruct.ipass;   // interlaced starts at 1
  }

  if (!mGIFStruct.interlaced) {
    MOZ_ASSERT(!mDeinterlacer);
    mGIFStruct.irow++;
  } else {
    static const uint8_t kjump[5] = { 1, 8, 8, 4, 2 };
    int currentPass = mGIFStruct.ipass;

    do {
      // Row increments resp. per 8,8,4,2 rows
      mGIFStruct.irow += kjump[mGIFStruct.ipass];
      if (mGIFStruct.irow >= mGIFStruct.height) {
        // Next pass starts resp. at row 4,2,1,0
        mGIFStruct.irow = 8 >> mGIFStruct.ipass;
        mGIFStruct.ipass++;
      }
    } while (mGIFStruct.irow >= mGIFStruct.height);

    // We've finished a pass. If we're downscaling, it's time to propagate the
    // rows we've decoded so far from our Deinterlacer to our Downscaler.
    if (mGIFStruct.ipass > currentPass && mDownscaler) {
      MOZ_ASSERT(mDeinterlacer);
      mDeinterlacer->PropagatePassToDownscaler(*mDownscaler);
      FlushImageData();
      mDownscaler->ResetForNextProgressivePass();
    }
  }

  return --mGIFStruct.rows_remaining;
}

//******************************************************************************
// Perform Lempel-Ziv-Welch decoding
bool
nsGIFDecoder2::DoLzw(const uint8_t* q)
{
  if (!mGIFStruct.rows_remaining) {
    return true;
  }

  // Copy all the decoder state variables into locals so the compiler
  // won't worry about them being aliased.  The locals will be homed
  // back into the GIF decoder structure when we exit.
  int avail       = mGIFStruct.avail;
  int bits        = mGIFStruct.bits;
  int codesize    = mGIFStruct.codesize;
  int codemask    = mGIFStruct.codemask;
  int count       = mGIFStruct.count;
  int oldcode     = mGIFStruct.oldcode;
  const int clear_code = ClearCode();
  uint8_t firstchar = mGIFStruct.firstchar;
  int32_t datum     = mGIFStruct.datum;
  uint16_t* prefix  = mGIFStruct.prefix;
  uint8_t* stackp   = mGIFStruct.stackp;
  uint8_t* suffix   = mGIFStruct.suffix;
  uint8_t* stack    = mGIFStruct.stack;
  uint8_t* rowp     = mGIFStruct.rowp;

  uint8_t* rowend = GetCurrentRowBuffer() + mGIFStruct.width;

#define OUTPUT_ROW()                                        \
  PR_BEGIN_MACRO                                            \
    if (!OutputRow())                                       \
      goto END;                                             \
    rowp = GetCurrentRowBuffer();                           \
    rowend = rowp + mGIFStruct.width;                       \
  PR_END_MACRO

  for (const uint8_t* ch = q; count-- > 0; ch++) {
    // Feed the next byte into the decoder's 32-bit input buffer.
    datum += ((int32_t)* ch) << bits;
    bits += 8;

    // Check for underflow of decoder's 32-bit input buffer.
    while (bits >= codesize) {
      // Get the leading variable-length symbol from the data stream
      int code = datum & codemask;
      datum >>= codesize;
      bits -= codesize;

      // Reset the dictionary to its original state, if requested
      if (code == clear_code) {
        codesize = mGIFStruct.datasize + 1;
        codemask = (1 << codesize) - 1;
        avail = clear_code + 2;
        oldcode = -1;
        continue;
      }

      // Check for explicit end-of-stream code
      if (code == (clear_code + 1)) {
        // end-of-stream should only appear after all image data
        return (mGIFStruct.rows_remaining == 0);
      }

      if (oldcode == -1) {
        if (code >= MAX_BITS) {
          return false;
        }
        *rowp++ = suffix[code] & mColorMask; // ensure index is within colormap
        if (rowp == rowend) {
          OUTPUT_ROW();
        }

        firstchar = oldcode = code;
        continue;
      }

      int incode = code;
      if (code >= avail) {
        *stackp++ = firstchar;
        code = oldcode;

        if (stackp >= stack + MAX_BITS) {
          return false;
        }
      }

      while (code >= clear_code) {
        if ((code >= MAX_BITS) || (code == prefix[code])) {
          return false;
        }

        *stackp++ = suffix[code];
        code = prefix[code];

        if (stackp == stack + MAX_BITS) {
          return false;
        }
      }

      *stackp++ = firstchar = suffix[code];

      // Define a new codeword in the dictionary.
      if (avail < 4096) {
        prefix[avail] = oldcode;
        suffix[avail] = firstchar;
        avail++;

        // If we've used up all the codewords of a given length
        // increase the length of codewords by one bit, but don't
        // exceed the specified maximum codeword size of 12 bits.
        if (((avail & codemask) == 0) && (avail < 4096)) {
          codesize++;
          codemask += avail;
        }
      }
      oldcode = incode;

      // Copy the decoded data out to the scanline buffer.
      do {
        *rowp++ = *--stackp & mColorMask; // ensure index is within colormap
        if (rowp == rowend) {
          OUTPUT_ROW();
        }
      } while (stackp > stack);
    }
  }

  END:

  // Home the local copies of the GIF decoder state variables
  mGIFStruct.avail = avail;
  mGIFStruct.bits = bits;
  mGIFStruct.codesize = codesize;
  mGIFStruct.codemask = codemask;
  mGIFStruct.count = count;
  mGIFStruct.oldcode = oldcode;
  mGIFStruct.firstchar = firstchar;
  mGIFStruct.datum = datum;
  mGIFStruct.stackp = stackp;
  mGIFStruct.rowp = rowp;

  return true;
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

void
nsGIFDecoder2::WriteInternal(const char* aBuffer, uint32_t aCount)
{
  MOZ_ASSERT(!HasError(), "Shouldn't call WriteInternal after error!");

  // These variables changed names; renaming would make a much bigger patch :(
  const uint8_t* buf = (const uint8_t*)aBuffer;
  uint32_t len = aCount;

  const uint8_t* q = buf;

  // Add what we have sofar to the block
  // If previous call to me left something in the hold first complete current
  // block, or if we are filling the colormaps, first complete the colormap
  uint8_t* p =
    (mGIFStruct.state ==
      gif_global_colormap) ? (uint8_t*) mGIFStruct.global_colormap :
        (mGIFStruct.state == gif_image_colormap) ? (uint8_t*) mColormap :
          (mGIFStruct.bytes_in_hold) ? mGIFStruct.hold : nullptr;

  if (len == 0 && buf == nullptr) {
    // We've just gotten the frame we asked for. Time to use the data we
    // stashed away.
    len = mGIFStruct.bytes_in_hold;
    q = buf = p;
  } else if (p) {
    // Add what we have sofar to the block
    uint32_t l = std::min(len, mGIFStruct.bytes_to_consume);
    memcpy(p+mGIFStruct.bytes_in_hold, buf, l);

    if (l < mGIFStruct.bytes_to_consume) {
      // Not enough in 'buf' to complete current block, get more
      mGIFStruct.bytes_in_hold += l;
      mGIFStruct.bytes_to_consume -= l;
      return;
    }
    // Point 'q' to complete block in hold (or in colormap)
    q = p;
  }

  // Invariant:
  //    'q' is start of current to be processed block (hold, colormap or buf)
  //    'bytes_to_consume' is number of bytes to consume from 'buf'
  //    'buf' points to the bytes to be consumed from the input buffer
  //    'len' is number of bytes left in input buffer from position 'buf'.
  //    At entrance of the for loop will 'buf' will be moved 'bytes_to_consume'
  //    to point to next buffer, 'len' is adjusted accordingly.
  //    So that next round in for loop, q gets pointed to the next buffer.

  for (;len >= mGIFStruct.bytes_to_consume; q=buf, mGIFStruct.bytes_in_hold = 0)
  {
    // Eat the current block from the buffer, q keeps pointed at current block
    buf += mGIFStruct.bytes_to_consume;
    len -= mGIFStruct.bytes_to_consume;

    switch (mGIFStruct.state) {
    case gif_lzw:
      if (!DoLzw(q)) {
        mGIFStruct.state = gif_error;
        break;
      }
      GETN(1, gif_sub_block);
      break;

    case gif_lzw_start: {
      // Make sure the transparent pixel is transparent in the colormap
      if (mGIFStruct.is_transparent) {
        // Save old value so we can restore it later
        if (mColormap == mGIFStruct.global_colormap) {
            mOldColor = mColormap[mGIFStruct.tpixel];
        }
        mColormap[mGIFStruct.tpixel] = 0;
      }

      // Initialize LZW parser/decoder
      mGIFStruct.datasize = *q;
      const int clear_code = ClearCode();
      if (mGIFStruct.datasize > MAX_LZW_BITS ||
          clear_code >= MAX_BITS) {
        mGIFStruct.state = gif_error;
        break;
      }

      mGIFStruct.avail = clear_code + 2;
      mGIFStruct.oldcode = -1;
      mGIFStruct.codesize = mGIFStruct.datasize + 1;
      mGIFStruct.codemask = (1 << mGIFStruct.codesize) - 1;
      mGIFStruct.datum = mGIFStruct.bits = 0;

      // init the tables
      for (int i = 0; i < clear_code; i++) {
        mGIFStruct.suffix[i] = i;
      }

      mGIFStruct.stackp = mGIFStruct.stack;

      GETN(1, gif_sub_block);
    }
    break;

    // All GIF files begin with "GIF87a" or "GIF89a"
    case gif_type:
      if (!strncmp((char*)q, "GIF89a", 6)) {
        mGIFStruct.version = 89;
      } else if (!strncmp((char*)q, "GIF87a", 6)) {
        mGIFStruct.version = 87;
      } else {
        mGIFStruct.state = gif_error;
        break;
      }
      GETN(7, gif_global_header);
      break;

    case gif_global_header:
      // This is the height and width of the "screen" or
      // frame into which images are rendered.  The
      // individual images can be smaller than the
      // screen size and located with an origin anywhere
      // within the screen.

      mGIFStruct.screen_width = GETINT16(q);
      mGIFStruct.screen_height = GETINT16(q + 2);
      mGIFStruct.global_colormap_depth = (q[4]&0x07) + 1;

      // screen_bgcolor is not used
      //mGIFStruct.screen_bgcolor = q[5];
      // q[6] = Pixel Aspect Ratio
      //   Not used
      //   float aspect = (float)((q[6] + 15) / 64.0);

      if (q[4] & 0x80) {
        // Get the global colormap
        const uint32_t size = (3 << mGIFStruct.global_colormap_depth);
        if (len < size) {
          // Use 'hold' pattern to get the global colormap
          GETN(size, gif_global_colormap);
          break;
        }
        // Copy everything, go to colormap state to do CMS correction
        memcpy(mGIFStruct.global_colormap, buf, size);
        buf += size;
        len -= size;
        GETN(0, gif_global_colormap);
        break;
      }

      GETN(1, gif_image_start);
      break;

    case gif_global_colormap:
      // Everything is already copied into global_colormap
      // Convert into Cairo colors including CMS transformation
      ConvertColormap(mGIFStruct.global_colormap,
                      1<<mGIFStruct.global_colormap_depth);
      GETN(1, gif_image_start);
      break;

    case gif_image_start:
      switch (*q) {
        case GIF_TRAILER:
          if (IsMetadataDecode()) {
            return;
          }
          mGIFStruct.state = gif_done;
          break;

        case GIF_EXTENSION_INTRODUCER:
          GETN(2, gif_extension);
          break;

        case GIF_IMAGE_SEPARATOR:
          GETN(9, gif_image_header);
          break;

        default:
          // If we get anything other than GIF_IMAGE_SEPARATOR,
          // GIF_EXTENSION_INTRODUCER, or GIF_TRAILER, there is extraneous data
          // between blocks. The GIF87a spec tells us to keep reading
          // until we find an image separator, but GIF89a says such
          // a file is corrupt. We follow GIF89a and bail out.
          if (mGIFStruct.images_decoded > 0) {
            // The file is corrupt, but one or more images have
            // been decoded correctly. In this case, we proceed
            // as if the file were correctly terminated and set
            // the state to gif_done, so the GIF will display.
            mGIFStruct.state = gif_done;
          } else {
            // No images decoded, there is nothing to display.
            mGIFStruct.state = gif_error;
          }
      }
      break;

    case gif_extension:
      mGIFStruct.bytes_to_consume = q[1];
      if (mGIFStruct.bytes_to_consume) {
        switch (*q) {
        case GIF_GRAPHIC_CONTROL_LABEL:
          // The GIF spec mandates that the GIFControlExtension header block
          // length is 4 bytes, and the parser for this block reads 4 bytes,
          // so we must enforce that the buffer contains at least this many
          // bytes. If the GIF specifies a different length, we allow that, so
          // long as it's larger; the additional data will simply be ignored.
          mGIFStruct.state = gif_control_extension;
          mGIFStruct.bytes_to_consume =
            std::max(mGIFStruct.bytes_to_consume, 4u);
          break;

        // The GIF spec also specifies the lengths of the following two
        // extensions' headers (as 12 and 11 bytes, respectively). Because
        // we ignore the plain text extension entirely and sanity-check the
        // actual length of the application extension header before reading it,
        // we allow GIFs to deviate from these values in either direction. This
        // is important for real-world compatibility, as GIFs in the wild exist
        // with application extension headers that are both shorter and longer
        // than 11 bytes.
        case GIF_APPLICATION_EXTENSION_LABEL:
          mGIFStruct.state = gif_application_extension;
          break;

        case GIF_PLAIN_TEXT_LABEL:
          mGIFStruct.state = gif_skip_block;
          break;

        case GIF_COMMENT_LABEL:
          mGIFStruct.state = gif_consume_comment;
          break;

        default:
          mGIFStruct.state = gif_skip_block;
        }
      } else {
        GETN(1, gif_image_start);
      }
      break;

    case gif_consume_block:
      if (!*q) {
        GETN(1, gif_image_start);
      } else {
        GETN(*q, gif_skip_block);
      }
      break;

    case gif_skip_block:
      GETN(1, gif_consume_block);
      break;

    case gif_control_extension:
      mGIFStruct.is_transparent = *q & 0x1;
      mGIFStruct.tpixel = q[3];
      mGIFStruct.disposal_method = ((*q) >> 2) & 0x7;

      if (mGIFStruct.disposal_method == 4) {
        // Some specs say 3rd bit (value 4), other specs say value 3.
        // Let's choose 3 (the more popular).
        mGIFStruct.disposal_method = 3;
      } else if (mGIFStruct.disposal_method > 4) {
        // This GIF is using a disposal method which is undefined in the spec.
        // Treat it as DisposalMethod::NOT_SPECIFIED.
        mGIFStruct.disposal_method = 0;
      }

      {
        DisposalMethod method = DisposalMethod(mGIFStruct.disposal_method);
        if (method == DisposalMethod::CLEAR_ALL ||
            method == DisposalMethod::CLEAR) {
          // We may have to display the background under this image during
          // animation playback, so we regard it as transparent.
          PostHasTransparency();
        }
      }

      mGIFStruct.delay_time = GETINT16(q + 1) * 10;

      if (mGIFStruct.delay_time > 0) {
        PostIsAnimated(mGIFStruct.delay_time);
      }

      GETN(1, gif_consume_block);
      break;

    case gif_comment_extension:
      if (*q) {
        GETN(*q, gif_consume_comment);
      } else {
        GETN(1, gif_image_start);
      }
      break;

    case gif_consume_comment:
      GETN(1, gif_comment_extension);
      break;

    case gif_application_extension:
      // Check for netscape application extension
      if (mGIFStruct.bytes_to_consume == 11 &&
          (!strncmp((char*)q, "NETSCAPE2.0", 11) ||
           !strncmp((char*)q, "ANIMEXTS1.0", 11))) {
        GETN(1, gif_netscape_extension_block);
      } else {
        GETN(1, gif_consume_block);
      }
      break;

    // Netscape-specific GIF extension: animation looping
    case gif_netscape_extension_block:
      if (*q) {
        // We might need to consume 3 bytes in
        // gif_consume_netscape_extension, so make sure we have at least that.
        GETN(std::max(3, static_cast<int>(*q)), gif_consume_netscape_extension);
      } else {
        GETN(1, gif_image_start);
      }
      break;

    // Parse netscape-specific application extensions
    case gif_consume_netscape_extension:
      switch (q[0] & 7) {
        case 1:
          // Loop entire animation specified # of times.  Only read the
          // loop count during the first iteration.
          mGIFStruct.loop_count = GETINT16(q + 1);
          GETN(1, gif_netscape_extension_block);
          break;

        case 2:
          // Wait for specified # of bytes to enter buffer

          // Don't do this, this extension doesn't exist (isn't used at all)
          // and doesn't do anything, as our streaming/buffering takes care
          // of it all...
          // See: http://semmix.pl/color/exgraf/eeg24.htm
          GETN(1, gif_netscape_extension_block);
          break;

        default:
          // 0,3-7 are yet to be defined netscape extension codes
          mGIFStruct.state = gif_error;
      }
      break;

    case gif_image_header: {
      if (mGIFStruct.images_decoded == 1) {
        if (!HasAnimation()) {
          // We should've already called PostIsAnimated(); this must be a
          // corrupt animated image with a first frame timeout of zero. Signal
          // that we're animated now, before the first-frame decode early exit
          // below, so that RasterImage can detect that this happened.
          PostIsAnimated(/* aFirstFrameTimeout = */ 0);
        }

        if (IsFirstFrameDecode()) {
          // We're about to get a second frame, but we only want the first. Stop
          // decoding now.
          mGIFStruct.state = gif_done;
          break;
        }

        if (mDownscaler) {
          MOZ_ASSERT_UNREACHABLE("Doing downscale-during-decode "
                                 "for an animated image?");
          mDownscaler.reset();
        }
      }

      // Get image offsets, with respect to the screen origin
      mGIFStruct.x_offset = GETINT16(q);
      mGIFStruct.y_offset = GETINT16(q + 2);

      // Get image width and height.
      mGIFStruct.width  = GETINT16(q + 4);
      mGIFStruct.height = GETINT16(q + 6);

      if (!mGIFStruct.images_decoded) {
        // Work around broken GIF files where the logical screen
        // size has weird width or height.  We assume that GIF87a
        // files don't contain animations.
        if ((mGIFStruct.screen_height < mGIFStruct.height) ||
            (mGIFStruct.screen_width < mGIFStruct.width) ||
            (mGIFStruct.version == 87)) {
          mGIFStruct.screen_height = mGIFStruct.height;
          mGIFStruct.screen_width = mGIFStruct.width;
          mGIFStruct.x_offset = 0;
          mGIFStruct.y_offset = 0;
        }
        // Create the image container with the right size.
        BeginGIF();
        if (HasError()) {
          // Setting the size led to an error.
          mGIFStruct.state = gif_error;
          return;
        }

        // If we were doing a metadata decode, we're done.
        if (IsMetadataDecode()) {
          IntRect frameRect(mGIFStruct.x_offset, mGIFStruct.y_offset,
                            mGIFStruct.width, mGIFStruct.height);
          CheckForTransparency(frameRect);
          return;
        }
      }

      // Work around more broken GIF files that have zero image width or height
      if (!mGIFStruct.height || !mGIFStruct.width) {
        mGIFStruct.height = mGIFStruct.screen_height;
        mGIFStruct.width = mGIFStruct.screen_width;
        if (!mGIFStruct.height || !mGIFStruct.width) {
          mGIFStruct.state = gif_error;
          break;
        }
      }

      // Depth of colors is determined by colormap
      // (q[8] & 0x80) indicates local colormap
      // bits per pixel is (q[8]&0x07 + 1) when local colormap is set
      uint32_t depth = mGIFStruct.global_colormap_depth;
      if (q[8] & 0x80) {
        depth = (q[8]&0x07) + 1;
      }
      uint32_t realDepth = depth;
      while (mGIFStruct.tpixel >= (1 << realDepth) && (realDepth < 8)) {
        realDepth++;
      }
      // Mask to limit the color values within the colormap
      mColorMask = 0xFF >> (8 - realDepth);

      if (NS_FAILED(BeginImageFrame(realDepth))) {
        mGIFStruct.state = gif_error;
        return;
      }

      // FALL THROUGH
    }

    case gif_image_header_continue: {
      // While decoders can reuse frames, we unconditionally increment
      // mGIFStruct.images_decoded when we're done with a frame, so we both can
      // and need to zero out the colormap and image data after every new frame.
      memset(mImageData, 0, mImageDataLength);
      if (mColormap) {
        memset(mColormap, 0, mColormapSize);
      }

      if (!mGIFStruct.images_decoded) {
        // Send a onetime invalidation for the first frame if it has a y-axis
        // offset. Otherwise, the area may never be refreshed and the
        // placeholder will remain on the screen. (Bug 37589)
        if (mGIFStruct.y_offset > 0) {
          if (mDownscaler) {
            IntRect targetRect = IntRect(IntPoint(), mDownscaler->TargetSize());
            PostInvalidation(IntRect(IntPoint(), GetSize()), Some(targetRect));
          } else {
            nsIntRect r(0, 0, mGIFStruct.screen_width, mGIFStruct.y_offset);
            PostInvalidation(r);
          }
        }
      }

      if (q[8] & 0x40) {
        mGIFStruct.interlaced = true;
        mGIFStruct.ipass = 1;
        if (mDownscaler) {
          mDeinterlacer.emplace(mDownscaler->OriginalSize());
        }
      } else {
        mGIFStruct.interlaced = false;
        mGIFStruct.ipass = 0;
      }

      // Only apply the Haeberli display hack on the first frame
      mGIFStruct.progressive_display = (mGIFStruct.images_decoded == 0);

      // Clear state from last image
      mGIFStruct.irow = 0;
      mGIFStruct.rows_remaining = mGIFStruct.height;
      mGIFStruct.rowp = GetCurrentRowBuffer();

      // Depth of colors is determined by colormap
      // (q[8] & 0x80) indicates local colormap
      // bits per pixel is (q[8]&0x07 + 1) when local colormap is set
      uint32_t depth = mGIFStruct.global_colormap_depth;
      if (q[8] & 0x80) {
        depth = (q[8]&0x07) + 1;
      }
      uint32_t realDepth = depth;
      while (mGIFStruct.tpixel >= (1 << realDepth) && (realDepth < 8)) {
        realDepth++;
      }
      // has a local colormap?
      if (q[8] & 0x80) {
        mGIFStruct.local_colormap_size = 1 << depth;
        if (!mGIFStruct.images_decoded) {
          // First frame has local colormap, allocate space for it
          // as the image frame doesn't have its own palette
          mColormapSize = sizeof(uint32_t) << realDepth;
          if (!mGIFStruct.local_colormap) {
            mGIFStruct.local_colormap = (uint32_t*)moz_xmalloc(mColormapSize);
          }
          mColormap = mGIFStruct.local_colormap;
        }
        const uint32_t size = 3 << depth;
        if (mColormapSize > size) {
          // Clear the notfilled part of the colormap
          memset(((uint8_t*)mColormap) + size, 0, mColormapSize - size);
        }
        if (len < size) {
          // Use 'hold' pattern to get the image colormap
          GETN(size, gif_image_colormap);
          break;
        }
        // Copy everything, go to colormap state to do CMS correction
        memcpy(mColormap, buf, size);
        buf += size;
        len -= size;
        GETN(0, gif_image_colormap);
        break;
      } else {
        // Switch back to the global palette
        if (mGIFStruct.images_decoded) {
          // Copy global colormap into the palette of current frame
          memcpy(mColormap, mGIFStruct.global_colormap, mColormapSize);
        } else {
          mColormap = mGIFStruct.global_colormap;
        }
      }
      GETN(1, gif_lzw_start);
    }
    break;

    case gif_image_colormap:
      // Everything is already copied into local_colormap
      // Convert into Cairo colors including CMS transformation
      ConvertColormap(mColormap, mGIFStruct.local_colormap_size);
      GETN(1, gif_lzw_start);
      break;

    case gif_sub_block:
      mGIFStruct.count = *q;
      if (mGIFStruct.count) {
        // Still working on the same image: Process next LZW data block
        // Make sure there are still rows left. If the GIF data
        // is corrupt, we may not get an explicit terminator.
        if (!mGIFStruct.rows_remaining) {
#ifdef DONT_TOLERATE_BROKEN_GIFS
          mGIFStruct.state = gif_error;
          break;
#else
          // This is an illegal GIF, but we remain tolerant.
          GETN(1, gif_sub_block);
#endif
          if (mGIFStruct.count == GIF_TRAILER) {
            // Found a terminator anyway, so consider the image done
            GETN(1, gif_done);
            break;
          }
        }
        GETN(mGIFStruct.count, gif_lzw);
      } else {
        // See if there are any more images in this sequence.
        EndImageFrame();
        GETN(1, gif_image_start);
      }
      break;

    case gif_done:
      MOZ_ASSERT(!IsMetadataDecode(),
                 "Metadata decodes shouldn't reach gif_done");
      FinishInternal();
      goto done;

    case gif_error:
      PostDataError();
      return;

    // We shouldn't ever get here.
    default:
      MOZ_ASSERT_UNREACHABLE("Unexpected mGIFStruct.state");
      PostDecoderError(NS_ERROR_UNEXPECTED);
      return;
    }
  }

  // if an error state is set but no data remains, code flow reaches here
  if (mGIFStruct.state == gif_error) {
      PostDataError();
      return;
  }

  // Copy the leftover into mGIFStruct.hold
  if (len) {
    // Add what we have sofar to the block
    if (mGIFStruct.state != gif_global_colormap &&
        mGIFStruct.state != gif_image_colormap) {
      if (!SetHold(buf, len)) {
        PostDataError();
        return;
      }
    } else {
      uint8_t* p = (mGIFStruct.state == gif_global_colormap) ?
                    (uint8_t*)mGIFStruct.global_colormap :
                    (uint8_t*)mColormap;
      memcpy(p, buf, len);
      mGIFStruct.bytes_in_hold = len;
    }

    mGIFStruct.bytes_to_consume -= len;
  }

// We want to flush before returning if we're on the first frame
done:
  if (!mGIFStruct.images_decoded) {
    FlushImageData();
    mLastFlushedRow = mCurrentRow;
    mLastFlushedPass = mCurrentPass;
  }
}

bool
nsGIFDecoder2::SetHold(const uint8_t* buf1, uint32_t count1,
                       const uint8_t* buf2 /* = nullptr */,
                       uint32_t count2 /* = 0 */)
{
  // We have to handle the case that buf currently points to hold
  uint8_t* newHold = (uint8_t*) malloc(std::max(uint32_t(MIN_HOLD_SIZE),
                                       count1 + count2));
  if (!newHold) {
    mGIFStruct.state = gif_error;
    return false;
  }

  memcpy(newHold, buf1, count1);
  if (buf2) {
    memcpy(newHold + count1, buf2, count2);
  }

  free(mGIFStruct.hold);
  mGIFStruct.hold = newHold;
  mGIFStruct.bytes_in_hold = count1 + count2;
  return true;
}

Telemetry::ID
nsGIFDecoder2::SpeedHistogram()
{
  return Telemetry::IMAGE_DECODE_SPEED_GIF;
}

} // namespace image
} // namespace mozilla
