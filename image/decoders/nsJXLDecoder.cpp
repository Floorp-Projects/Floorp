/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageLogging.h"  // Must appear first
#include "gfxPlatform.h"
#include "jxl/codestream_header.h"
#include "jxl/decode_cxx.h"
#include "jxl/types.h"
#include "mozilla/TelemetryHistogramEnums.h"
#include "mozilla/gfx/Point.h"
#include "nsJXLDecoder.h"

#include "RasterImage.h"
#include "SurfacePipeFactory.h"

using namespace mozilla::gfx;

namespace mozilla::image {

#define JXL_TRY(expr)                        \
  do {                                       \
    JxlDecoderStatus _status = (expr);       \
    if (_status != JXL_DEC_SUCCESS) {        \
      return Transition::TerminateFailure(); \
    }                                        \
  } while (0);

#define JXL_TRY_BOOL(expr)                   \
  do {                                       \
    bool succeeded = (expr);                 \
    if (!succeeded) {                        \
      return Transition::TerminateFailure(); \
    }                                        \
  } while (0);

static LazyLogModule sJXLLog("JXLDecoder");

nsJXLDecoder::nsJXLDecoder(RasterImage* aImage)
    : Decoder(aImage),
      mLexer(Transition::ToUnbuffered(State::FINISHED_JXL_DATA, State::JXL_DATA,
                                      SIZE_MAX),
             Transition::TerminateSuccess()),
      mDecoder(JxlDecoderMake(nullptr)),
      mParallelRunner(
          JxlThreadParallelRunnerMake(nullptr, PreferredThreadCount())),
      mUsePipeTransform(true),
      mCMSLine(nullptr) {
  int events = JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE;

  if (mCMSMode != CMSMode::Off) {
    events |= JXL_DEC_COLOR_ENCODING;
  }

  JxlDecoderSubscribeEvents(mDecoder.get(), events);
  JxlDecoderSetParallelRunner(mDecoder.get(), JxlThreadParallelRunner,
                              mParallelRunner.get());

  MOZ_LOG(sJXLLog, LogLevel::Debug,
          ("[this=%p] nsJXLDecoder::nsJXLDecoder", this));
}

nsJXLDecoder::~nsJXLDecoder() {
  MOZ_LOG(sJXLLog, LogLevel::Debug,
          ("[this=%p] nsJXLDecoder::~nsJXLDecoder", this));

  if (mCMSLine) {
    free(mCMSLine);
  }
}

size_t nsJXLDecoder::PreferredThreadCount() {
  if (IsMetadataDecode()) {
    return 0;  // no additional worker thread
  }
  return JxlThreadParallelRunnerDefaultNumWorkerThreads();
}

LexerResult nsJXLDecoder::DoDecode(SourceBufferIterator& aIterator,
                                   IResumable* aOnResume) {
  // return LexerResult(TerminalState::FAILURE);
  MOZ_ASSERT(!HasError(), "Shouldn't call DoDecode after error!");

  return mLexer.Lex(aIterator, aOnResume,
                    [=](State aState, const char* aData, size_t aLength) {
                      switch (aState) {
                        case State::JXL_DATA:
                          return ReadJXLData(aData, aLength);
                        case State::FINISHED_JXL_DATA:
                          return FinishedJXLData();
                      }
                      MOZ_CRASH("Unknown State");
                    });
};

LexerTransition<nsJXLDecoder::State> nsJXLDecoder::ReadJXLData(
    const char* aData, size_t aLength) {
  const uint8_t* input = (const uint8_t*)aData;
  size_t length = aLength;
  if (mBuffer.length() != 0) {
    JXL_TRY_BOOL(mBuffer.append(aData, aLength));
    input = mBuffer.begin();
    length = mBuffer.length();
  }
  JXL_TRY(JxlDecoderSetInput(mDecoder.get(), input, length));

  while (true) {
    JxlDecoderStatus status = JxlDecoderProcessInput(mDecoder.get());
    switch (status) {
      case JXL_DEC_ERROR:
      default:
        return Transition::TerminateFailure();

      case JXL_DEC_NEED_MORE_INPUT: {
        size_t remaining = JxlDecoderReleaseInput(mDecoder.get());
        mBuffer.clear();
        JXL_TRY_BOOL(mBuffer.append(aData + aLength - remaining, remaining));
        return Transition::ContinueUnbuffered(State::JXL_DATA);
      }

      case JXL_DEC_BASIC_INFO: {
        JXL_TRY(JxlDecoderGetBasicInfo(mDecoder.get(), &mInfo));
        PostSize(mInfo.xsize, mInfo.ysize);

        if (mInfo.alpha_bits > 0) {
          PostHasTransparency();
        }

        if (IsMetadataDecode()) {
          return Transition::TerminateSuccess();
        }

        // If CMS is off or the image is RGB, always output in RGBA.
        // If the image is grayscale, then the pipe transform can't be used.
        if (mCMSMode != CMSMode::Off) {
          mChannels = mInfo.num_color_channels == 1
                          ? 1 + (mInfo.alpha_bits > 0 ? 1 : 0)
                          : 4;
        } else {
          mChannels = 4;
        }

        mFormat = {mChannels, JXL_TYPE_UINT8, JXL_LITTLE_ENDIAN, 0};

        break;
      }

      case JXL_DEC_NEED_IMAGE_OUT_BUFFER: {
        size_t size = 0;
        JXL_TRY(JxlDecoderImageOutBufferSize(mDecoder.get(), &mFormat, &size));

        mOutBuffer.clear();
        JXL_TRY_BOOL(mOutBuffer.growBy(size));
        JXL_TRY(JxlDecoderSetImageOutBuffer(mDecoder.get(), &mFormat,
                                            mOutBuffer.begin(), size));
        break;
      }

      case JXL_DEC_COLOR_ENCODING: {
        size_t size = 0;
        JXL_TRY(JxlDecoderGetICCProfileSize(
            mDecoder.get(), &mFormat, JXL_COLOR_PROFILE_TARGET_DATA, &size))
        std::vector<uint8_t> icc_profile(size);
        JXL_TRY(JxlDecoderGetColorAsICCProfile(mDecoder.get(), &mFormat,
                                               JXL_COLOR_PROFILE_TARGET_DATA,
                                               icc_profile.data(), size))

        mInProfile = qcms_profile_from_memory((char*)icc_profile.data(), size);

        uint32_t profileSpace = qcms_profile_get_color_space(mInProfile);

        // Skip color management if color profile is not compatible with number
        // of channels.
        if (profileSpace != icSigRgbData &&
            (mInfo.num_color_channels == 3 || profileSpace != icSigGrayData)) {
          break;
        }

        mUsePipeTransform =
            profileSpace == icSigRgbData && mInfo.num_color_channels == 3;

        qcms_data_type inType;
        if (mInfo.num_color_channels == 3) {
          inType = QCMS_DATA_RGBA_8;
        } else if (mInfo.alpha_bits > 0) {
          inType = QCMS_DATA_GRAYA_8;
        } else {
          inType = QCMS_DATA_GRAY_8;
        }

        if (!mUsePipeTransform) {
          mCMSLine =
              static_cast<uint8_t*>(malloc(sizeof(uint32_t) * mInfo.xsize));
        }

        int intent = gfxPlatform::GetRenderingIntent();
        if (intent == -1) {
          intent = qcms_profile_get_rendering_intent(mInProfile);
        }

        mTransform =
            qcms_transform_create(mInProfile, inType, GetCMSOutputProfile(),
                                  QCMS_DATA_RGBA_8, (qcms_intent)intent);

        break;
      }

      case JXL_DEC_FULL_IMAGE: {
        OrientedIntSize size(mInfo.xsize, mInfo.ysize);

        qcms_transform* pipeTransform =
            mUsePipeTransform ? mTransform : nullptr;

        Maybe<SurfacePipe> pipe = SurfacePipeFactory::CreateSurfacePipe(
            this, size, OutputSize(), FullFrame(), SurfaceFormat::R8G8B8A8,
            SurfaceFormat::OS_RGBA, Nothing(), pipeTransform,
            SurfacePipeFlags());

        for (uint8_t* rowPtr = mOutBuffer.begin(); rowPtr < mOutBuffer.end();
             rowPtr += mInfo.xsize * mChannels) {
          uint8_t* rowToWrite = rowPtr;

          if (!mUsePipeTransform && mTransform) {
            qcms_transform_data(mTransform, rowToWrite, mCMSLine, mInfo.xsize);
            rowToWrite = mCMSLine;
          }

          pipe->WriteBuffer(reinterpret_cast<uint32_t*>(rowToWrite));
        }

        if (Maybe<SurfaceInvalidRect> invalidRect = pipe->TakeInvalidRect()) {
          PostInvalidation(invalidRect->mInputSpaceRect,
                           Some(invalidRect->mOutputSpaceRect));
        }
        PostFrameStop();
        PostDecodeDone();
        return Transition::TerminateSuccess();
      }
    }
  }
}

LexerTransition<nsJXLDecoder::State> nsJXLDecoder::FinishedJXLData() {
  MOZ_ASSERT_UNREACHABLE("Read the entire address space?");
  return Transition::TerminateFailure();
}

}  // namespace mozilla::image
