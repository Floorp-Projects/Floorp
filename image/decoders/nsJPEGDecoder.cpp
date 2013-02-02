/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageLogging.h"
#include "nsJPEGDecoder.h"

#include "nsIInputStream.h"

#include "nspr.h"
#include "nsCRT.h"
#include "gfxColor.h"

#include "jerror.h"

#include "gfxPlatform.h"

extern "C" {
#include "iccjpeg.h"
}

#if defined(IS_BIG_ENDIAN)
#define MOZ_JCS_EXT_NATIVE_ENDIAN_XRGB JCS_EXT_XRGB
#else
#define MOZ_JCS_EXT_NATIVE_ENDIAN_XRGB JCS_EXT_BGRX
#endif

static void cmyk_convert_rgb(JSAMPROW row, JDIMENSION width);

namespace mozilla {
namespace image {

#if defined(PR_LOGGING)
static PRLogModuleInfo *
GetJPEGLog()
{
  static PRLogModuleInfo *sJPEGLog;
  if (!sJPEGLog)
    sJPEGLog = PR_NewLogModule("JPEGDecoder");
  return sJPEGLog;
}

static PRLogModuleInfo *
GetJPEGDecoderAccountingLog()
{
  static PRLogModuleInfo *sJPEGDecoderAccountingLog;
  if (!sJPEGDecoderAccountingLog)
    sJPEGDecoderAccountingLog = PR_NewLogModule("JPEGDecoderAccounting");
  return sJPEGDecoderAccountingLog;
}
#else
#define GetJPEGLog()
#define GetJPEGDecoderAccountingLog()
#endif

static qcms_profile*
GetICCProfile(struct jpeg_decompress_struct &info)
{
  JOCTET* profilebuf;
  uint32_t profileLength;
  qcms_profile* profile = nullptr;

  if (read_icc_profile(&info, &profilebuf, &profileLength)) {
    profile = qcms_profile_from_memory(profilebuf, profileLength);
    free(profilebuf);
  }

  return profile;
}

METHODDEF(void) init_source (j_decompress_ptr jd);
METHODDEF(boolean) fill_input_buffer (j_decompress_ptr jd);
METHODDEF(void) skip_input_data (j_decompress_ptr jd, long num_bytes);
METHODDEF(void) term_source (j_decompress_ptr jd);
METHODDEF(void) my_error_exit (j_common_ptr cinfo);

/* Normal JFIF markers can't have more bytes than this. */
#define MAX_JPEG_MARKER_LENGTH  (((uint32_t)1 << 16) - 1)


nsJPEGDecoder::nsJPEGDecoder(RasterImage& aImage, Decoder::DecodeStyle aDecodeStyle)
 : Decoder(aImage)
 , mDecodeStyle(aDecodeStyle)
{
  mState = JPEG_HEADER;
  mReading = true;
  mImageData = nullptr;

  mBytesToSkip = 0;
  memset(&mInfo, 0, sizeof(jpeg_decompress_struct));
  memset(&mSourceMgr, 0, sizeof(mSourceMgr));
  mInfo.client_data = (void*)this;

  mSegment = nullptr;
  mSegmentLen = 0;

  mBackBuffer = nullptr;
  mBackBufferLen = mBackBufferSize = mBackBufferUnreadLen = 0;

  mInProfile = nullptr;
  mTransform = nullptr;

  mCMSMode = 0;

  PR_LOG(GetJPEGDecoderAccountingLog(), PR_LOG_DEBUG,
         ("nsJPEGDecoder::nsJPEGDecoder: Creating JPEG decoder %p",
          this));
}

nsJPEGDecoder::~nsJPEGDecoder()
{
  // Step 8: Release JPEG decompression object
  mInfo.src = nullptr;
  jpeg_destroy_decompress(&mInfo);

  PR_FREEIF(mBackBuffer);
  if (mTransform)
    qcms_transform_release(mTransform);
  if (mInProfile)
    qcms_profile_release(mInProfile);

  PR_LOG(GetJPEGDecoderAccountingLog(), PR_LOG_DEBUG,
         ("nsJPEGDecoder::~nsJPEGDecoder: Destroying JPEG decoder %p",
          this));
}

Telemetry::ID
nsJPEGDecoder::SpeedHistogram()
{
  return Telemetry::IMAGE_DECODE_SPEED_JPEG;
}

void
nsJPEGDecoder::InitInternal()
{
  mCMSMode = gfxPlatform::GetCMSMode();
  if ((mDecodeFlags & DECODER_NO_COLORSPACE_CONVERSION) != 0)
    mCMSMode = eCMSMode_Off;

  /* We set up the normal JPEG error routines, then override error_exit. */
  mInfo.err = jpeg_std_error(&mErr.pub);
  /*   mInfo.err = jpeg_std_error(&mErr.pub); */
  mErr.pub.error_exit = my_error_exit;
  /* Establish the setjmp return context for my_error_exit to use. */
  if (setjmp(mErr.setjmp_buffer)) {
    /* If we get here, the JPEG code has signaled an error.
     * We need to clean up the JPEG object, close the input file, and return.
     */
    PostDecoderError(NS_ERROR_FAILURE);
    return;
  }

  /* Step 1: allocate and initialize JPEG decompression object */
  jpeg_create_decompress(&mInfo);
  /* Set the source manager */
  mInfo.src = &mSourceMgr;

  /* Step 2: specify data source (eg, a file) */

  /* Setup callback functions. */
  mSourceMgr.init_source = init_source;
  mSourceMgr.fill_input_buffer = fill_input_buffer;
  mSourceMgr.skip_input_data = skip_input_data;
  mSourceMgr.resync_to_restart = jpeg_resync_to_restart;
  mSourceMgr.term_source = term_source;

  /* Record app markers for ICC data */
  for (uint32_t m = 0; m < 16; m++)
    jpeg_save_markers(&mInfo, JPEG_APP0 + m, 0xFFFF);
}

void
nsJPEGDecoder::FinishInternal()
{
  /* If we're not in any sort of error case, flush the decoder.
   *
   * XXXbholley - It seems wrong that this should be necessary, but at the
   * moment I'm just folding the contents of Flush() into Close() so that
   * we can get rid of it.
   */
  if ((mState != JPEG_DONE && mState != JPEG_SINK_NON_JPEG_TRAILER) &&
      (mState != JPEG_ERROR) &&
      !IsSizeDecode())
    this->Write(nullptr, 0);
}

void
nsJPEGDecoder::WriteInternal(const char *aBuffer, uint32_t aCount)
{
  mSegment = (const JOCTET *)aBuffer;
  mSegmentLen = aCount;

  NS_ABORT_IF_FALSE(!HasError(), "Shouldn't call WriteInternal after error!");

  /* Return here if there is a fatal error within libjpeg. */
  nsresult error_code;
  // This cast to nsresult makes sense because setjmp() returns whatever we
  // passed to longjmp(), which was actually an nsresult.
  if ((error_code = (nsresult)setjmp(mErr.setjmp_buffer)) != NS_OK) {
    if (error_code == NS_ERROR_FAILURE) {
      PostDataError();
      /* Error due to corrupt stream - return NS_OK and consume silently
         so that libpr0n doesn't throw away a partial image load */
      mState = JPEG_SINK_NON_JPEG_TRAILER;
      PR_LOG(GetJPEGDecoderAccountingLog(), PR_LOG_DEBUG,
             ("} (setjmp returned NS_ERROR_FAILURE)"));
      return;
    } else {
      /* Error due to reasons external to the stream (probably out of
         memory) - let libpr0n attempt to clean up, even though
         mozilla is seconds away from falling flat on its face. */
      PostDecoderError(error_code);
      mState = JPEG_ERROR;
      PR_LOG(GetJPEGDecoderAccountingLog(), PR_LOG_DEBUG,
             ("} (setjmp returned an error)"));
      return;
    }
  }

  PR_LOG(GetJPEGLog(), PR_LOG_DEBUG,
         ("[this=%p] nsJPEGDecoder::Write -- processing JPEG data\n", this));

  switch (mState) {
  case JPEG_HEADER:
  {
    LOG_SCOPE(GetJPEGLog(), "nsJPEGDecoder::Write -- entering JPEG_HEADER case");

    /* Step 3: read file parameters with jpeg_read_header() */
    if (jpeg_read_header(&mInfo, TRUE) == JPEG_SUSPENDED) {
      PR_LOG(GetJPEGDecoderAccountingLog(), PR_LOG_DEBUG,
             ("} (JPEG_SUSPENDED)"));
      return; /* I/O suspension */
    }

    // Post our size to the superclass
    PostSize(mInfo.image_width, mInfo.image_height);
    if (HasError()) {
      // Setting the size led to an error.
      mState = JPEG_ERROR;
      return;
    }

    /* If we're doing a size decode, we're done. */
    if (IsSizeDecode())
      return;

    /* We're doing a full decode. */
    if (mCMSMode != eCMSMode_Off &&
        (mInProfile = GetICCProfile(mInfo)) != nullptr) {
      uint32_t profileSpace = qcms_profile_get_color_space(mInProfile);
      bool mismatch = false;

#ifdef DEBUG_tor
      fprintf(stderr, "JPEG profileSpace: 0x%08X\n", profileSpace);
#endif
      switch (mInfo.jpeg_color_space) {
      case JCS_GRAYSCALE:
        if (profileSpace == icSigRgbData)
          mInfo.out_color_space = JCS_RGB;
        else if (profileSpace != icSigGrayData)
          mismatch = true;
        break;
      case JCS_RGB:
        if (profileSpace != icSigRgbData)
          mismatch =  true;
        break;
      case JCS_YCbCr:
        if (profileSpace == icSigRgbData)
          mInfo.out_color_space = JCS_RGB;
        else
	  // qcms doesn't support ycbcr
          mismatch = true;
        break;
      case JCS_CMYK:
      case JCS_YCCK:
	  // qcms doesn't support cmyk
          mismatch = true;
        break;
      default:
        mState = JPEG_ERROR;
        PostDataError();
        PR_LOG(GetJPEGDecoderAccountingLog(), PR_LOG_DEBUG,
               ("} (unknown colorpsace (1))"));
        return;
      }

      if (!mismatch) {
        qcms_data_type type;
        switch (mInfo.out_color_space) {
        case JCS_GRAYSCALE:
          type = QCMS_DATA_GRAY_8;
          break;
        case JCS_RGB:
          type = QCMS_DATA_RGB_8;
          break;
        default:
          mState = JPEG_ERROR;
          PostDataError();
          PR_LOG(GetJPEGDecoderAccountingLog(), PR_LOG_DEBUG,
                 ("} (unknown colorpsace (2))"));
          return;
        }
#if 0
        /* We don't currently support CMYK profiles. The following
         * code dealt with lcms types. Add something like this
         * back when we gain support for CMYK.
         */
        /* Adobe Photoshop writes YCCK/CMYK files with inverted data */
        if (mInfo.out_color_space == JCS_CMYK)
          type |= FLAVOR_SH(mInfo.saw_Adobe_marker ? 1 : 0);
#endif

        if (gfxPlatform::GetCMSOutputProfile()) {

          /* Calculate rendering intent. */
          int intent = gfxPlatform::GetRenderingIntent();
          if (intent == -1)
              intent = qcms_profile_get_rendering_intent(mInProfile);

          /* Create the color management transform. */
          mTransform = qcms_transform_create(mInProfile,
                                          type,
                                          gfxPlatform::GetCMSOutputProfile(),
                                          QCMS_DATA_RGB_8,
                                          (qcms_intent)intent);
        }
      } else {
#ifdef DEBUG_tor
        fprintf(stderr, "ICM profile colorspace mismatch\n");
#endif
      }
    }

    if (!mTransform) {
      switch (mInfo.jpeg_color_space) {
      case JCS_GRAYSCALE:
      case JCS_RGB:
      case JCS_YCbCr:
        // if we're not color managing we can decode directly to
        // MOZ_JCS_EXT_NATIVE_ENDIAN_XRGB
        if (mCMSMode != eCMSMode_All) {
            mInfo.out_color_space = MOZ_JCS_EXT_NATIVE_ENDIAN_XRGB;
            mInfo.out_color_components = 4;
        } else {
            mInfo.out_color_space = JCS_RGB;
        }
        break;
      case JCS_CMYK:
      case JCS_YCCK:
        /* libjpeg can convert from YCCK to CMYK, but not to RGB */
        mInfo.out_color_space = JCS_CMYK;
        break;
      default:
        mState = JPEG_ERROR;
        PostDataError();
        PR_LOG(GetJPEGDecoderAccountingLog(), PR_LOG_DEBUG,
               ("} (unknown colorpsace (3))"));
        return;
        break;
      }
    }

    /*
     * Don't allocate a giant and superfluous memory buffer
     * when not doing a progressive decode.
     */
    mInfo.buffered_image = mDecodeStyle == PROGRESSIVE && jpeg_has_multiple_scans(&mInfo);

    /* Used to set up image size so arrays can be allocated */
    jpeg_calc_output_dimensions(&mInfo);

    if (!mImageData) {
      mState = JPEG_ERROR;
      PostDecoderError(NS_ERROR_OUT_OF_MEMORY);
      PR_LOG(GetJPEGDecoderAccountingLog(), PR_LOG_DEBUG,
             ("} (could not initialize image frame)"));
      return;
    }

    PR_LOG(GetJPEGDecoderAccountingLog(), PR_LOG_DEBUG,
           ("        JPEGDecoderAccounting: nsJPEGDecoder::Write -- created image frame with %ux%u pixels",
            mInfo.image_width, mInfo.image_height));

    mState = JPEG_START_DECOMPRESS;
  }

  case JPEG_START_DECOMPRESS:
  {
    LOG_SCOPE(GetJPEGLog(), "nsJPEGDecoder::Write -- entering JPEG_START_DECOMPRESS case");
    /* Step 4: set parameters for decompression */

    /* FIXME -- Should reset dct_method and dither mode
     * for final pass of progressive JPEG
     */
    mInfo.dct_method =  JDCT_ISLOW;
    mInfo.dither_mode = JDITHER_FS;
    mInfo.do_fancy_upsampling = TRUE;
    mInfo.enable_2pass_quant = FALSE;
    mInfo.do_block_smoothing = TRUE;

    /* Step 5: Start decompressor */
    if (jpeg_start_decompress(&mInfo) == FALSE) {
      PR_LOG(GetJPEGDecoderAccountingLog(), PR_LOG_DEBUG,
             ("} (I/O suspension after jpeg_start_decompress())"));
      return; /* I/O suspension */
    }


    /* If this is a progressive JPEG ... */
    mState = mInfo.buffered_image ? JPEG_DECOMPRESS_PROGRESSIVE : JPEG_DECOMPRESS_SEQUENTIAL;
  }

  case JPEG_DECOMPRESS_SEQUENTIAL:
  {
    if (mState == JPEG_DECOMPRESS_SEQUENTIAL)
    {
      LOG_SCOPE(GetJPEGLog(), "nsJPEGDecoder::Write -- JPEG_DECOMPRESS_SEQUENTIAL case");
      
      bool suspend;
      OutputScanlines(&suspend);
      
      if (suspend) {
        PR_LOG(GetJPEGDecoderAccountingLog(), PR_LOG_DEBUG,
               ("} (I/O suspension after OutputScanlines() - SEQUENTIAL)"));
        return; /* I/O suspension */
      }
      
      /* If we've completed image output ... */
      NS_ASSERTION(mInfo.output_scanline == mInfo.output_height, "We didn't process all of the data!");
      mState = JPEG_DONE;
    }
  }

  case JPEG_DECOMPRESS_PROGRESSIVE:
  {
    if (mState == JPEG_DECOMPRESS_PROGRESSIVE)
    {
      LOG_SCOPE(GetJPEGLog(), "nsJPEGDecoder::Write -- JPEG_DECOMPRESS_PROGRESSIVE case");

      int status;
      do {
        status = jpeg_consume_input(&mInfo);
      } while ((status != JPEG_SUSPENDED) &&
               (status != JPEG_REACHED_EOI));

      for (;;) {
        if (mInfo.output_scanline == 0) {
          int scan = mInfo.input_scan_number;

          /* if we haven't displayed anything yet (output_scan_number==0)
             and we have enough data for a complete scan, force output
             of the last full scan */
          if ((mInfo.output_scan_number == 0) &&
              (scan > 1) &&
              (status != JPEG_REACHED_EOI))
            scan--;

          if (!jpeg_start_output(&mInfo, scan)) {
            PR_LOG(GetJPEGDecoderAccountingLog(), PR_LOG_DEBUG,
                   ("} (I/O suspension after jpeg_start_output() - PROGRESSIVE)"));
            return; /* I/O suspension */
          }
        }

        if (mInfo.output_scanline == 0xffffff)
          mInfo.output_scanline = 0;

        bool suspend;
        OutputScanlines(&suspend);

        if (suspend) {
          if (mInfo.output_scanline == 0) {
            /* didn't manage to read any lines - flag so we don't call
               jpeg_start_output() multiple times for the same scan */
            mInfo.output_scanline = 0xffffff;
          }
          PR_LOG(GetJPEGDecoderAccountingLog(), PR_LOG_DEBUG,
                 ("} (I/O suspension after OutputScanlines() - PROGRESSIVE)"));
          return; /* I/O suspension */
        }

        if (mInfo.output_scanline == mInfo.output_height)
        {
          if (!jpeg_finish_output(&mInfo)) {
            PR_LOG(GetJPEGDecoderAccountingLog(), PR_LOG_DEBUG,
                   ("} (I/O suspension after jpeg_finish_output() - PROGRESSIVE)"));
            return; /* I/O suspension */
          }

          if (jpeg_input_complete(&mInfo) &&
              (mInfo.input_scan_number == mInfo.output_scan_number))
            break;

          mInfo.output_scanline = 0;
        }
      }

      mState = JPEG_DONE;
    }
  }

  case JPEG_DONE:
  {
    LOG_SCOPE(GetJPEGLog(), "nsJPEGDecoder::ProcessData -- entering JPEG_DONE case");

    /* Step 7: Finish decompression */

    if (jpeg_finish_decompress(&mInfo) == FALSE) {
      PR_LOG(GetJPEGDecoderAccountingLog(), PR_LOG_DEBUG,
             ("} (I/O suspension after jpeg_finish_decompress() - DONE)"));
      return; /* I/O suspension */
    }

    mState = JPEG_SINK_NON_JPEG_TRAILER;

    /* we're done dude */
    break;
  }
  case JPEG_SINK_NON_JPEG_TRAILER:
    PR_LOG(GetJPEGLog(), PR_LOG_DEBUG,
           ("[this=%p] nsJPEGDecoder::ProcessData -- entering JPEG_SINK_NON_JPEG_TRAILER case\n", this));

    break;

  case JPEG_ERROR:
    NS_ABORT_IF_FALSE(0, "Should always return immediately after error and not re-enter decoder");
  }

  PR_LOG(GetJPEGDecoderAccountingLog(), PR_LOG_DEBUG,
         ("} (end of function)"));
  return;
}

void
nsJPEGDecoder::NotifyDone()
{
  PostFrameStop(RasterImage::kFrameOpaque);
  PostDecodeDone();
}

void
nsJPEGDecoder::OutputScanlines(bool* suspend)
{
  *suspend = false;

  const uint32_t top = mInfo.output_scanline;

  while ((mInfo.output_scanline < mInfo.output_height)) {
      /* Use the Cairo image buffer as scanline buffer */
      uint32_t *imageRow = ((uint32_t*)mImageData) +
                           (mInfo.output_scanline * mInfo.output_width);

      if (mInfo.out_color_space == MOZ_JCS_EXT_NATIVE_ENDIAN_XRGB) {
        /* Special case: scanline will be directly converted into packed ARGB */
        if (jpeg_read_scanlines(&mInfo, (JSAMPARRAY)&imageRow, 1) != 1) {
          *suspend = true; /* suspend */
          break;
        }
        continue; /* all done for this row! */
      }

      JSAMPROW sampleRow = (JSAMPROW)imageRow;
      if (mInfo.output_components == 3) {
        /* Put the pixels at end of row to enable in-place expansion */
        sampleRow += mInfo.output_width;
      }

      /* Request one scanline.  Returns 0 or 1 scanlines. */    
      if (jpeg_read_scanlines(&mInfo, &sampleRow, 1) != 1) {
        *suspend = true; /* suspend */
        break;
      }

      if (mTransform) {
        JSAMPROW source = sampleRow;
        if (mInfo.out_color_space == JCS_GRAYSCALE) {
          /* Convert from the 1byte grey pixels at begin of row 
             to the 3byte RGB byte pixels at 'end' of row */
          sampleRow += mInfo.output_width;
        }
        qcms_transform_data(mTransform, source, sampleRow, mInfo.output_width);
        /* Move 3byte RGB data to end of row */
        if (mInfo.out_color_space == JCS_CMYK) {
          memmove(sampleRow + mInfo.output_width,
                  sampleRow,
                  3 * mInfo.output_width);
          sampleRow += mInfo.output_width;
        }
      } else {
        if (mInfo.out_color_space == JCS_CMYK) {
          /* Convert from CMYK to RGB */
          /* We cannot convert directly to Cairo, as the CMSRGBTransform may wants to do a RGB transform... */
          /* Would be better to have platform CMSenabled transformation from CMYK to (A)RGB... */
          cmyk_convert_rgb((JSAMPROW)imageRow, mInfo.output_width);
          sampleRow += mInfo.output_width;
        }
        if (mCMSMode == eCMSMode_All) {
          /* No embedded ICC profile - treat as sRGB */
          qcms_transform *transform = gfxPlatform::GetCMSRGBTransform();
          if (transform) {
            qcms_transform_data(transform, sampleRow, sampleRow, mInfo.output_width);
          }
        }
      }

      // counter for while() loops below
      uint32_t idx = mInfo.output_width;

      // copy as bytes until source pointer is 32-bit-aligned
      for (; (NS_PTR_TO_UINT32(sampleRow) & 0x3) && idx; --idx) {
        *imageRow++ = gfxPackedPixel(0xFF, sampleRow[0], sampleRow[1], sampleRow[2]);
        sampleRow += 3;
      }

      // copy pixels in blocks of 4
      while (idx >= 4) {
        GFX_BLOCK_RGB_TO_FRGB(sampleRow, imageRow);
        idx       -=  4;
        sampleRow += 12;
        imageRow  +=  4;
      }

      // copy remaining pixel(s)
      while (idx--) {
        // 32-bit read of final pixel will exceed buffer, so read bytes
        *imageRow++ = gfxPackedPixel(0xFF, sampleRow[0], sampleRow[1], sampleRow[2]);
        sampleRow += 3;
      }
  }

  if (top != mInfo.output_scanline) {
      nsIntRect r(0, top, mInfo.output_width, mInfo.output_scanline-top);
      PostInvalidation(r);
  }

}


/* Override the standard error method in the IJG JPEG decoder code. */
METHODDEF(void)
my_error_exit (j_common_ptr cinfo)
{
  decoder_error_mgr *err = (decoder_error_mgr *) cinfo->err;

  /* Convert error to a browser error code */
  nsresult error_code = err->pub.msg_code == JERR_OUT_OF_MEMORY
                      ? NS_ERROR_OUT_OF_MEMORY
                      : NS_ERROR_FAILURE;

#ifdef DEBUG
  char buffer[JMSG_LENGTH_MAX];

  /* Create the message */
  (*err->pub.format_message) (cinfo, buffer);

  fprintf(stderr, "JPEG decoding error:\n%s\n", buffer);
#endif

  /* Return control to the setjmp point.  We pass an nsresult masquerading as
   * an int, which works because the setjmp() caller casts it back. */
  longjmp(err->setjmp_buffer, static_cast<int>(error_code));
}

/******************************************************************************/
/*-----------------------------------------------------------------------------
 * This is the callback routine from the IJG JPEG library used to supply new
 * data to the decompressor when its input buffer is exhausted.  It juggles
 * multiple buffers in an attempt to avoid unnecessary copying of input data.
 *
 * (A simpler scheme is possible: It's much easier to use only a single
 * buffer; when fill_input_buffer() is called, move any unconsumed data
 * (beyond the current pointer/count) down to the beginning of this buffer and
 * then load new data into the remaining buffer space.  This approach requires
 * a little more data copying but is far easier to get right.)
 *
 * At any one time, the JPEG decompressor is either reading from the necko
 * input buffer, which is volatile across top-level calls to the IJG library,
 * or the "backtrack" buffer.  The backtrack buffer contains the remaining
 * unconsumed data from the necko buffer after parsing was suspended due
 * to insufficient data in some previous call to the IJG library.
 *
 * When suspending, the decompressor will back up to a convenient restart
 * point (typically the start of the current MCU). The variables
 * next_input_byte & bytes_in_buffer indicate where the restart point will be
 * if the current call returns FALSE.  Data beyond this point must be
 * rescanned after resumption, so it must be preserved in case the decompressor
 * decides to backtrack.
 *
 * Returns:
 *  TRUE if additional data is available, FALSE if no data present and
 *   the JPEG library should therefore suspend processing of input stream
 *---------------------------------------------------------------------------*/

/******************************************************************************/
/* data source manager method                                                 */
/******************************************************************************/


/******************************************************************************/
/* data source manager method 
        Initialize source.  This is called by jpeg_read_header() before any
        data is actually read.  May leave
        bytes_in_buffer set to 0 (in which case a fill_input_buffer() call
        will occur immediately).
*/
METHODDEF(void)
init_source (j_decompress_ptr jd)
{
}

/******************************************************************************/
/* data source manager method
        Skip num_bytes worth of data.  The buffer pointer and count should
        be advanced over num_bytes input bytes, refilling the buffer as
        needed.  This is used to skip over a potentially large amount of
        uninteresting data (such as an APPn marker).  In some applications
        it may be possible to optimize away the reading of the skipped data,
        but it's not clear that being smart is worth much trouble; large
        skips are uncommon.  bytes_in_buffer may be zero on return.
        A zero or negative skip count should be treated as a no-op.
*/
METHODDEF(void)
skip_input_data (j_decompress_ptr jd, long num_bytes)
{
  struct jpeg_source_mgr *src = jd->src;
  nsJPEGDecoder *decoder = (nsJPEGDecoder *)(jd->client_data);

  if (num_bytes > (long)src->bytes_in_buffer) {
    /*
     * Can't skip it all right now until we get more data from
     * network stream. Set things up so that fill_input_buffer
     * will skip remaining amount.
     */
    decoder->mBytesToSkip = (size_t)num_bytes - src->bytes_in_buffer;
    src->next_input_byte += src->bytes_in_buffer;
    src->bytes_in_buffer = 0;

  } else {
    /* Simple case. Just advance buffer pointer */

    src->bytes_in_buffer -= (size_t)num_bytes;
    src->next_input_byte += num_bytes;
  }
}


/******************************************************************************/
/* data source manager method
        This is called whenever bytes_in_buffer has reached zero and more
        data is wanted.  In typical applications, it should read fresh data
        into the buffer (ignoring the current state of next_input_byte and
        bytes_in_buffer), reset the pointer & count to the start of the
        buffer, and return TRUE indicating that the buffer has been reloaded.
        It is not necessary to fill the buffer entirely, only to obtain at
        least one more byte.  bytes_in_buffer MUST be set to a positive value
        if TRUE is returned.  A FALSE return should only be used when I/O
        suspension is desired.
*/
METHODDEF(boolean)
fill_input_buffer (j_decompress_ptr jd)
{
  struct jpeg_source_mgr *src = jd->src;
  nsJPEGDecoder *decoder = (nsJPEGDecoder *)(jd->client_data);

  if (decoder->mReading) {
    const JOCTET *new_buffer = decoder->mSegment;
    uint32_t new_buflen = decoder->mSegmentLen;
  
    if (!new_buffer || new_buflen == 0)
      return false; /* suspend */

    decoder->mSegmentLen = 0;

    if (decoder->mBytesToSkip) {
      if (decoder->mBytesToSkip < new_buflen) {
        /* All done skipping bytes; Return what's left. */
        new_buffer += decoder->mBytesToSkip;
        new_buflen -= decoder->mBytesToSkip;
        decoder->mBytesToSkip = 0;
      } else {
        /* Still need to skip some more data in the future */
        decoder->mBytesToSkip -= (size_t)new_buflen;
        return false; /* suspend */
      }
    }

      decoder->mBackBufferUnreadLen = src->bytes_in_buffer;

    src->next_input_byte = new_buffer;
    src->bytes_in_buffer = (size_t)new_buflen;
    decoder->mReading = false;

    return true;
  }

  if (src->next_input_byte != decoder->mSegment) {
    /* Backtrack data has been permanently consumed. */
    decoder->mBackBufferUnreadLen = 0;
    decoder->mBackBufferLen = 0;
  }

  /* Save remainder of netlib buffer in backtrack buffer */
  const uint32_t new_backtrack_buflen = src->bytes_in_buffer + decoder->mBackBufferLen;
 
  /* Make sure backtrack buffer is big enough to hold new data. */
  if (decoder->mBackBufferSize < new_backtrack_buflen) {
    /* Check for malformed MARKER segment lengths, before allocating space for it */
    if (new_backtrack_buflen > MAX_JPEG_MARKER_LENGTH) {
      my_error_exit((j_common_ptr)(&decoder->mInfo));
    }

    /* Round up to multiple of 256 bytes. */
    const size_t roundup_buflen = ((new_backtrack_buflen + 255) >> 8) << 8;
    JOCTET *buf = (JOCTET *)PR_REALLOC(decoder->mBackBuffer, roundup_buflen);
    /* Check for OOM */
    if (!buf) {
      decoder->mInfo.err->msg_code = JERR_OUT_OF_MEMORY;
      my_error_exit((j_common_ptr)(&decoder->mInfo));
    }
    decoder->mBackBuffer = buf;
    decoder->mBackBufferSize = roundup_buflen;
  }

  /* Copy remainder of netlib segment into backtrack buffer. */
  memmove(decoder->mBackBuffer + decoder->mBackBufferLen,
          src->next_input_byte,
          src->bytes_in_buffer);

  /* Point to start of data to be rescanned. */
  src->next_input_byte = decoder->mBackBuffer + decoder->mBackBufferLen - decoder->mBackBufferUnreadLen;
  src->bytes_in_buffer += decoder->mBackBufferUnreadLen;
  decoder->mBackBufferLen = (size_t)new_backtrack_buflen;
  decoder->mReading = true;

  return false;
}

/******************************************************************************/
/* data source manager method */
/*
 * Terminate source --- called by jpeg_finish_decompress() after all
 * data has been read to clean up JPEG source manager. NOT called by 
 * jpeg_abort() or jpeg_destroy().
 */
METHODDEF(void)
term_source (j_decompress_ptr jd)
{
  nsJPEGDecoder *decoder = (nsJPEGDecoder *)(jd->client_data);

  // This function shouldn't be called if we ran into an error we didn't
  // recover from.
  NS_ABORT_IF_FALSE(decoder->mState != JPEG_ERROR,
                    "Calling term_source on a JPEG with mState == JPEG_ERROR!");

  // Notify using a helper method to get around protectedness issues.
  decoder->NotifyDone();
}

} // namespace image
} // namespace mozilla


/**************** Inverted CMYK -> RGB conversion **************/
/*
 * Input is (Inverted) CMYK stored as 4 bytes per pixel.
 * Output is RGB stored as 3 bytes per pixel.
 * @param row Points to row buffer containing the CMYK bytes for each pixel in the row.
 * @param width Number of pixels in the row.
 */
static void cmyk_convert_rgb(JSAMPROW row, JDIMENSION width)
{
  /* Work from end to front to shrink from 4 bytes per pixel to 3 */
  JSAMPROW in = row + width*4;
  JSAMPROW out = in;

  for (uint32_t i = width; i > 0; i--) {
    in -= 4;
    out -= 3;

    // Source is 'Inverted CMYK', output is RGB.
    // See: http://www.easyrgb.com/math.php?MATH=M12#text12
    // Or:  http://www.ilkeratalay.com/colorspacesfaq.php#rgb

    // From CMYK to CMY
    // C = ( C * ( 1 - K ) + K )
    // M = ( M * ( 1 - K ) + K )
    // Y = ( Y * ( 1 - K ) + K )

    // From Inverted CMYK to CMY is thus:
    // C = ( (1-iC) * (1 - (1-iK)) + (1-iK) ) => 1 - iC*iK
    // Same for M and Y

    // Convert from CMY (0..1) to RGB (0..1)
    // R = 1 - C => 1 - (1 - iC*iK) => iC*iK
    // G = 1 - M => 1 - (1 - iM*iK) => iM*iK
    // B = 1 - Y => 1 - (1 - iY*iK) => iY*iK
  
    // Convert from Inverted CMYK (0..255) to RGB (0..255)
    const uint32_t iC = in[0];
    const uint32_t iM = in[1];
    const uint32_t iY = in[2];
    const uint32_t iK = in[3];
    out[0] = iC*iK/255;   // Red
    out[1] = iM*iK/255;   // Green
    out[2] = iY*iK/255;   // Blue
  }
}
