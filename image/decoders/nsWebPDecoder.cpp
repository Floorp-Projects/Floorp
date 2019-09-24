/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageLogging.h"  // Must appear first
#include "nsWebPDecoder.h"

#include "RasterImage.h"
#include "SurfacePipeFactory.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace image {

static LazyLogModule sWebPLog("WebPDecoder");

nsWebPDecoder::nsWebPDecoder(RasterImage* aImage)
    : Decoder(aImage),
      mDecoder(nullptr),
      mBlend(BlendMethod::OVER),
      mDisposal(DisposalMethod::KEEP),
      mTimeout(FrameTimeout::Forever()),
      mFormat(SurfaceFormat::B8G8R8X8),
      mLastRow(0),
      mCurrentFrame(0),
      mData(nullptr),
      mLength(0),
      mIteratorComplete(false),
      mNeedDemuxer(true),
      mGotColorProfile(false) {
  MOZ_LOG(sWebPLog, LogLevel::Debug,
          ("[this=%p] nsWebPDecoder::nsWebPDecoder", this));
}

nsWebPDecoder::~nsWebPDecoder() {
  MOZ_LOG(sWebPLog, LogLevel::Debug,
          ("[this=%p] nsWebPDecoder::~nsWebPDecoder", this));
  if (mDecoder) {
    WebPIDelete(mDecoder);
    WebPFreeDecBuffer(&mBuffer);
  }
}

LexerResult nsWebPDecoder::ReadData() {
  MOZ_ASSERT(mData);
  MOZ_ASSERT(mLength > 0);

  WebPDemuxer* demuxer = nullptr;
  bool complete = mIteratorComplete;

  if (mNeedDemuxer) {
    WebPDemuxState state;
    WebPData fragment;
    fragment.bytes = mData;
    fragment.size = mLength;

    demuxer = WebPDemuxPartial(&fragment, &state);
    if (state == WEBP_DEMUX_PARSE_ERROR) {
      MOZ_LOG(
          sWebPLog, LogLevel::Error,
          ("[this=%p] nsWebPDecoder::ReadData -- demux parse error\n", this));
      WebPDemuxDelete(demuxer);
      return LexerResult(TerminalState::FAILURE);
    }

    if (state == WEBP_DEMUX_PARSING_HEADER) {
      WebPDemuxDelete(demuxer);
      return LexerResult(Yield::NEED_MORE_DATA);
    }

    if (!demuxer) {
      MOZ_LOG(sWebPLog, LogLevel::Error,
              ("[this=%p] nsWebPDecoder::ReadData -- no demuxer\n", this));
      return LexerResult(TerminalState::FAILURE);
    }

    complete = complete || state == WEBP_DEMUX_DONE;
  }

  LexerResult rv(TerminalState::FAILURE);
  if (!HasSize()) {
    rv = ReadHeader(demuxer, complete);
  } else {
    rv = ReadPayload(demuxer, complete);
  }

  WebPDemuxDelete(demuxer);
  return rv;
}

LexerResult nsWebPDecoder::DoDecode(SourceBufferIterator& aIterator,
                                    IResumable* aOnResume) {
  while (true) {
    SourceBufferIterator::State state = SourceBufferIterator::COMPLETE;
    if (!mIteratorComplete) {
      state = aIterator.AdvanceOrScheduleResume(SIZE_MAX, aOnResume);

      // We need to remember since we can't advance a complete iterator.
      mIteratorComplete = state == SourceBufferIterator::COMPLETE;
    }

    if (state == SourceBufferIterator::WAITING) {
      return LexerResult(Yield::NEED_MORE_DATA);
    }

    LexerResult rv = UpdateBuffer(aIterator, state);
    if (rv.is<Yield>() && rv.as<Yield>() == Yield::NEED_MORE_DATA) {
      // We need to check the iterator to see if more is available before
      // giving up unless we are already complete.
      if (mIteratorComplete) {
        MOZ_LOG(sWebPLog, LogLevel::Error,
                ("[this=%p] nsWebPDecoder::DoDecode -- read all data, "
                 "but needs more\n",
                 this));
        return LexerResult(TerminalState::FAILURE);
      }
      continue;
    }

    return rv;
  }
}

LexerResult nsWebPDecoder::UpdateBuffer(SourceBufferIterator& aIterator,
                                        SourceBufferIterator::State aState) {
  MOZ_ASSERT(!HasError(), "Shouldn't call DoDecode after error!");

  switch (aState) {
    case SourceBufferIterator::READY:
      if (!aIterator.IsContiguous()) {
        // We need to buffer. This should be rare, but expensive.
        break;
      }
      if (!mData) {
        // For as long as we hold onto an iterator, we know the data pointers
        // to the chunks cannot change underneath us, so save the pointer to
        // the first block.
        MOZ_ASSERT(mLength == 0);
        mData = reinterpret_cast<const uint8_t*>(aIterator.Data());
      }
      mLength += aIterator.Length();
      return ReadData();
    case SourceBufferIterator::COMPLETE:
      return ReadData();
    default:
      MOZ_LOG(sWebPLog, LogLevel::Error,
              ("[this=%p] nsWebPDecoder::DoDecode -- bad state\n", this));
      return LexerResult(TerminalState::FAILURE);
  }

  // We need to buffer. If we have no data buffered, we need to get everything
  // from the first chunk of the source buffer before appending the new data.
  if (mBufferedData.empty()) {
    MOZ_ASSERT(mData);
    MOZ_ASSERT(mLength > 0);

    if (!mBufferedData.append(mData, mLength)) {
      MOZ_LOG(sWebPLog, LogLevel::Error,
              ("[this=%p] nsWebPDecoder::DoDecode -- oom, initialize %zu\n",
               this, mLength));
      return LexerResult(TerminalState::FAILURE);
    }

    MOZ_LOG(sWebPLog, LogLevel::Debug,
            ("[this=%p] nsWebPDecoder::DoDecode -- buffered %zu bytes\n", this,
             mLength));
  }

  // Append the incremental data from the iterator.
  if (!mBufferedData.append(aIterator.Data(), aIterator.Length())) {
    MOZ_LOG(sWebPLog, LogLevel::Error,
            ("[this=%p] nsWebPDecoder::DoDecode -- oom, append %zu on %zu\n",
             this, aIterator.Length(), mBufferedData.length()));
    return LexerResult(TerminalState::FAILURE);
  }

  MOZ_LOG(sWebPLog, LogLevel::Debug,
          ("[this=%p] nsWebPDecoder::DoDecode -- buffered %zu -> %zu bytes\n",
           this, aIterator.Length(), mBufferedData.length()));
  mData = mBufferedData.begin();
  mLength = mBufferedData.length();
  return ReadData();
}

nsresult nsWebPDecoder::CreateFrame(const nsIntRect& aFrameRect) {
  MOZ_ASSERT(HasSize());
  MOZ_ASSERT(!mDecoder);

  MOZ_LOG(
      sWebPLog, LogLevel::Debug,
      ("[this=%p] nsWebPDecoder::CreateFrame -- frame %u, (%d, %d) %d x %d\n",
       this, mCurrentFrame, aFrameRect.x, aFrameRect.y, aFrameRect.width,
       aFrameRect.height));

  if (aFrameRect.width <= 0 || aFrameRect.height <= 0) {
    MOZ_LOG(sWebPLog, LogLevel::Error,
            ("[this=%p] nsWebPDecoder::CreateFrame -- bad frame rect\n", this));
    return NS_ERROR_FAILURE;
  }

  // If this is our first frame in an animation and it doesn't cover the
  // full frame, then we are transparent even if there is no alpha
  if (mCurrentFrame == 0 && !aFrameRect.IsEqualEdges(FullFrame())) {
    MOZ_ASSERT(HasAnimation());
    mFormat = SurfaceFormat::B8G8R8A8;
    PostHasTransparency();
  }

  WebPInitDecBuffer(&mBuffer);
  mBuffer.colorspace = MODE_BGRA;

  mDecoder = WebPINewDecoder(&mBuffer);
  if (!mDecoder) {
    MOZ_LOG(sWebPLog, LogLevel::Error,
            ("[this=%p] nsWebPDecoder::CreateFrame -- create decoder error\n",
             this));
    return NS_ERROR_FAILURE;
  }

  // WebP doesn't guarantee that the alpha generated matches the hint in the
  // header, so we always need to claim the input is BGRA. If the output is
  // BGRX, swizzling will mask off the alpha channel.
  SurfaceFormat inFormat = SurfaceFormat::B8G8R8A8;

  SurfacePipeFlags pipeFlags = SurfacePipeFlags();
  if (mFormat == SurfaceFormat::B8G8R8A8 &&
      !(GetSurfaceFlags() & SurfaceFlags::NO_PREMULTIPLY_ALPHA)) {
    pipeFlags |= SurfacePipeFlags::PREMULTIPLY_ALPHA;
  }

  Maybe<AnimationParams> animParams;
  if (!IsFirstFrameDecode()) {
    animParams.emplace(aFrameRect, mTimeout, mCurrentFrame, mBlend, mDisposal);
  }

  Maybe<SurfacePipe> pipe = SurfacePipeFactory::CreateSurfacePipe(
      this, Size(), OutputSize(), aFrameRect, inFormat, mFormat, animParams,
      mTransform, pipeFlags);
  if (!pipe) {
    MOZ_LOG(sWebPLog, LogLevel::Error,
            ("[this=%p] nsWebPDecoder::CreateFrame -- no pipe\n", this));
    return NS_ERROR_FAILURE;
  }

  mFrameRect = aFrameRect;
  mPipe = std::move(*pipe);
  return NS_OK;
}

void nsWebPDecoder::EndFrame() {
  MOZ_ASSERT(HasSize());
  MOZ_ASSERT(mDecoder);

  auto opacity = mFormat == SurfaceFormat::B8G8R8A8 ? Opacity::SOME_TRANSPARENCY
                                                    : Opacity::FULLY_OPAQUE;

  MOZ_LOG(sWebPLog, LogLevel::Debug,
          ("[this=%p] nsWebPDecoder::EndFrame -- frame %u, opacity %d, "
           "disposal %d, timeout %d, blend %d\n",
           this, mCurrentFrame, (int)opacity, (int)mDisposal,
           mTimeout.AsEncodedValueDeprecated(), (int)mBlend));

  PostFrameStop(opacity);
  WebPIDelete(mDecoder);
  WebPFreeDecBuffer(&mBuffer);
  mDecoder = nullptr;
  mLastRow = 0;
  ++mCurrentFrame;
}

void nsWebPDecoder::ApplyColorProfile(const char* aProfile, size_t aLength) {
  MOZ_ASSERT(!mGotColorProfile);
  mGotColorProfile = true;

  if (GetSurfaceFlags() & SurfaceFlags::NO_COLORSPACE_CONVERSION) {
    return;
  }

  auto mode = gfxPlatform::GetCMSMode();
  if (mode == eCMSMode_Off || (mode == eCMSMode_TaggedOnly && !aProfile) ||
      !gfxPlatform::GetCMSOutputProfile()) {
    return;
  }

  if (!aProfile) {
    MOZ_LOG(sWebPLog, LogLevel::Debug,
            ("[this=%p] nsWebPDecoder::ApplyColorProfile -- not tagged, use "
             "sRGB transform\n",
             this));
    mTransform = gfxPlatform::GetCMSBGRATransform();
    return;
  }

  mInProfile = qcms_profile_from_memory(aProfile, aLength);
  if (!mInProfile) {
    MOZ_LOG(
        sWebPLog, LogLevel::Error,
        ("[this=%p] nsWebPDecoder::ApplyColorProfile -- bad color profile\n",
         this));
    return;
  }

  uint32_t profileSpace = qcms_profile_get_color_space(mInProfile);
  if (profileSpace != icSigRgbData) {
    // WebP doesn't produce grayscale data, this must be corrupt.
    MOZ_LOG(sWebPLog, LogLevel::Error,
            ("[this=%p] nsWebPDecoder::ApplyColorProfile -- ignoring non-rgb "
             "color profile\n",
             this));
    return;
  }

  // Calculate rendering intent.
  int intent = gfxPlatform::GetRenderingIntent();
  if (intent == -1) {
    intent = qcms_profile_get_rendering_intent(mInProfile);
  }

  // Create the color management transform.
  mTransform = qcms_transform_create(mInProfile, QCMS_DATA_BGRA_8,
                                     gfxPlatform::GetCMSOutputProfile(),
                                     QCMS_DATA_BGRA_8, (qcms_intent)intent);
  MOZ_LOG(sWebPLog, LogLevel::Debug,
          ("[this=%p] nsWebPDecoder::ApplyColorProfile -- use tagged "
           "transform\n",
           this));
}

LexerResult nsWebPDecoder::ReadHeader(WebPDemuxer* aDemuxer, bool aIsComplete) {
  MOZ_ASSERT(aDemuxer);

  MOZ_LOG(
      sWebPLog, LogLevel::Debug,
      ("[this=%p] nsWebPDecoder::ReadHeader -- %zu bytes\n", this, mLength));

  uint32_t flags = WebPDemuxGetI(aDemuxer, WEBP_FF_FORMAT_FLAGS);

  if (!IsMetadataDecode() && !mGotColorProfile) {
    if (flags & WebPFeatureFlags::ICCP_FLAG) {
      WebPChunkIterator iter;
      if (!WebPDemuxGetChunk(aDemuxer, "ICCP", 1, &iter)) {
        return aIsComplete ? LexerResult(TerminalState::FAILURE)
                           : LexerResult(Yield::NEED_MORE_DATA);
      }

      ApplyColorProfile(reinterpret_cast<const char*>(iter.chunk.bytes),
                        iter.chunk.size);
      WebPDemuxReleaseChunkIterator(&iter);
    } else {
      ApplyColorProfile(nullptr, 0);
    }
  }

  if (flags & WebPFeatureFlags::ANIMATION_FLAG) {
    // A metadata decode expects to get the correct first frame timeout which
    // sadly is not provided by the normal WebP header parsing.
    WebPIterator iter;
    if (!WebPDemuxGetFrame(aDemuxer, 1, &iter)) {
      return aIsComplete ? LexerResult(TerminalState::FAILURE)
                         : LexerResult(Yield::NEED_MORE_DATA);
    }

    PostIsAnimated(FrameTimeout::FromRawMilliseconds(iter.duration));
    WebPDemuxReleaseIterator(&iter);
  } else {
    // Single frames don't need a demuxer to be created.
    mNeedDemuxer = false;
  }

  uint32_t width = WebPDemuxGetI(aDemuxer, WEBP_FF_CANVAS_WIDTH);
  uint32_t height = WebPDemuxGetI(aDemuxer, WEBP_FF_CANVAS_HEIGHT);
  if (width > INT32_MAX || height > INT32_MAX) {
    return LexerResult(TerminalState::FAILURE);
  }

  PostSize(width, height);

  bool alpha = flags & WebPFeatureFlags::ALPHA_FLAG;
  if (alpha) {
    mFormat = SurfaceFormat::B8G8R8A8;
    PostHasTransparency();
  }

  MOZ_LOG(sWebPLog, LogLevel::Debug,
          ("[this=%p] nsWebPDecoder::ReadHeader -- %u x %u, alpha %d, "
           "animation %d, metadata decode %d, first frame decode %d\n",
           this, width, height, alpha, HasAnimation(), IsMetadataDecode(),
           IsFirstFrameDecode()));

  if (IsMetadataDecode()) {
    return LexerResult(TerminalState::SUCCESS);
  }

  return ReadPayload(aDemuxer, aIsComplete);
}

LexerResult nsWebPDecoder::ReadPayload(WebPDemuxer* aDemuxer,
                                       bool aIsComplete) {
  if (!HasAnimation()) {
    auto rv = ReadSingle(mData, mLength, FullFrame());
    if (rv.is<TerminalState>() &&
        rv.as<TerminalState>() == TerminalState::SUCCESS) {
      PostDecodeDone();
    }
    return rv;
  }
  return ReadMultiple(aDemuxer, aIsComplete);
}

LexerResult nsWebPDecoder::ReadSingle(const uint8_t* aData, size_t aLength,
                                      const IntRect& aFrameRect) {
  MOZ_ASSERT(!IsMetadataDecode());
  MOZ_ASSERT(aData);
  MOZ_ASSERT(aLength > 0);

  MOZ_LOG(
      sWebPLog, LogLevel::Debug,
      ("[this=%p] nsWebPDecoder::ReadSingle -- %zu bytes\n", this, aLength));

  if (!mDecoder && NS_FAILED(CreateFrame(aFrameRect))) {
    return LexerResult(TerminalState::FAILURE);
  }

  bool complete;
  do {
    VP8StatusCode status = WebPIUpdate(mDecoder, aData, aLength);
    switch (status) {
      case VP8_STATUS_OK:
        complete = true;
        break;
      case VP8_STATUS_SUSPENDED:
        complete = false;
        break;
      default:
        MOZ_LOG(sWebPLog, LogLevel::Error,
                ("[this=%p] nsWebPDecoder::ReadSingle -- append error %d\n",
                 this, status));
        return LexerResult(TerminalState::FAILURE);
    }

    int lastRow = -1;
    int width = 0;
    int height = 0;
    int stride = 0;
    uint8_t* rowStart =
        WebPIDecGetRGB(mDecoder, &lastRow, &width, &height, &stride);

    MOZ_LOG(
        sWebPLog, LogLevel::Debug,
        ("[this=%p] nsWebPDecoder::ReadSingle -- complete %d, read %d rows, "
         "has %d rows available\n",
         this, complete, mLastRow, lastRow));

    if (!rowStart || lastRow == -1 || lastRow == mLastRow) {
      return LexerResult(Yield::NEED_MORE_DATA);
    }

    if (width != mFrameRect.width || height != mFrameRect.height ||
        stride < mFrameRect.width * 4 || lastRow > mFrameRect.height) {
      MOZ_LOG(sWebPLog, LogLevel::Error,
              ("[this=%p] nsWebPDecoder::ReadSingle -- bad (w,h,s) = (%d, %d, "
               "%d)\n",
               this, width, height, stride));
      return LexerResult(TerminalState::FAILURE);
    }

    for (int row = mLastRow; row < lastRow; row++) {
      uint32_t* src = reinterpret_cast<uint32_t*>(rowStart + row * stride);
      WriteState result = mPipe.WriteBuffer(src);

      Maybe<SurfaceInvalidRect> invalidRect = mPipe.TakeInvalidRect();
      if (invalidRect) {
        PostInvalidation(invalidRect->mInputSpaceRect,
                         Some(invalidRect->mOutputSpaceRect));
      }

      if (result == WriteState::FAILURE) {
        MOZ_LOG(sWebPLog, LogLevel::Error,
                ("[this=%p] nsWebPDecoder::ReadSingle -- write pixels error\n",
                 this));
        return LexerResult(TerminalState::FAILURE);
      }

      if (result == WriteState::FINISHED) {
        MOZ_ASSERT(row == lastRow - 1, "There was more data to read?");
        complete = true;
        break;
      }
    }

    mLastRow = lastRow;
  } while (!complete);

  if (!complete) {
    return LexerResult(Yield::NEED_MORE_DATA);
  }

  EndFrame();
  return LexerResult(TerminalState::SUCCESS);
}

LexerResult nsWebPDecoder::ReadMultiple(WebPDemuxer* aDemuxer,
                                        bool aIsComplete) {
  MOZ_ASSERT(!IsMetadataDecode());
  MOZ_ASSERT(aDemuxer);

  MOZ_LOG(sWebPLog, LogLevel::Debug,
          ("[this=%p] nsWebPDecoder::ReadMultiple\n", this));

  bool complete = aIsComplete;
  WebPIterator iter;
  auto rv = LexerResult(Yield::NEED_MORE_DATA);
  if (WebPDemuxGetFrame(aDemuxer, mCurrentFrame + 1, &iter)) {
    switch (iter.blend_method) {
      case WEBP_MUX_BLEND:
        mBlend = BlendMethod::OVER;
        break;
      case WEBP_MUX_NO_BLEND:
        mBlend = BlendMethod::SOURCE;
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("Unhandled blend method");
        break;
    }

    switch (iter.dispose_method) {
      case WEBP_MUX_DISPOSE_NONE:
        mDisposal = DisposalMethod::KEEP;
        break;
      case WEBP_MUX_DISPOSE_BACKGROUND:
        mDisposal = DisposalMethod::CLEAR;
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("Unhandled dispose method");
        break;
    }

    mFormat = iter.has_alpha || mCurrentFrame > 0 ? SurfaceFormat::B8G8R8A8
                                                  : SurfaceFormat::B8G8R8X8;
    mTimeout = FrameTimeout::FromRawMilliseconds(iter.duration);
    nsIntRect frameRect(iter.x_offset, iter.y_offset, iter.width, iter.height);

    rv = ReadSingle(iter.fragment.bytes, iter.fragment.size, frameRect);
    complete = complete && !WebPDemuxNextFrame(&iter);
    WebPDemuxReleaseIterator(&iter);
  }

  if (rv.is<TerminalState>() &&
      rv.as<TerminalState>() == TerminalState::SUCCESS) {
    // If we extracted one frame, and it is not the last, we need to yield to
    // the lexer to allow the upper layers to acknowledge the frame.
    if (!complete && !IsFirstFrameDecode()) {
      rv = LexerResult(Yield::OUTPUT_AVAILABLE);
    } else {
      uint32_t loopCount = WebPDemuxGetI(aDemuxer, WEBP_FF_LOOP_COUNT);

      MOZ_LOG(sWebPLog, LogLevel::Debug,
              ("[this=%p] nsWebPDecoder::ReadMultiple -- loop count %u\n", this,
               loopCount));
      PostDecodeDone(loopCount - 1);
    }
  }

  return rv;
}

Maybe<Telemetry::HistogramID> nsWebPDecoder::SpeedHistogram() const {
  return Some(Telemetry::IMAGE_DECODE_SPEED_WEBP);
}

}  // namespace image
}  // namespace mozilla
