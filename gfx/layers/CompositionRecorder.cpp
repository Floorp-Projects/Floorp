/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositionRecorder.h"
#include "gfxUtils.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/gfxVars.h"
#include "nsIInputStream.h"
#include "nsIBinaryOutputStream.h"
#include "nsIObjectOutputStream.h"
#include "prtime.h"

#include <ctime>
#include <iomanip>
#include "stdio.h"
#ifdef XP_WIN
#  include "direct.h"
#else
#  include <sys/types.h>
#  include "sys/stat.h"
#endif

using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

CompositionRecorder::CompositionRecorder(TimeStamp aRecordingStart)
    : mRecordingStart(aRecordingStart) {}

void CompositionRecorder::RecordFrame(RecordedFrame* aFrame) {
  mRecordedFrames.AppendElement(aFrame);
}

Maybe<FrameRecording> CompositionRecorder::GetRecording() {
  FrameRecording recording;

  recording.startTime() = mRecordingStart;

  nsTArray<uint8_t> bytes;
  for (RefPtr<RecordedFrame>& recordedFrame : mRecordedFrames) {
    RefPtr<DataSourceSurface> surf = recordedFrame->GetSourceSurface();
    if (!surf) {
      return Nothing();
    }

    nsCOMPtr<nsIInputStream> imgStream;
    nsresult rv = gfxUtils::EncodeSourceSurfaceAsStream(
        surf, ImageType::PNG, u""_ns, getter_AddRefs(imgStream));
    if (NS_FAILED(rv)) {
      return Nothing();
    }

    uint64_t bufSize64;
    rv = imgStream->Available(&bufSize64);
    if (NS_FAILED(rv) || bufSize64 > UINT32_MAX) {
      return Nothing();
    }

    const uint32_t frameLength = static_cast<uint32_t>(bufSize64);
    size_t startIndex = bytes.Length();
    bytes.SetLength(startIndex + frameLength);

    uint8_t* bytePtr = &bytes[startIndex];
    uint32_t bytesLeft = frameLength;

    while (bytesLeft > 0) {
      uint32_t bytesRead = 0;
      rv = imgStream->Read(reinterpret_cast<char*>(bytePtr), bytesLeft,
                           &bytesRead);
      if (NS_FAILED(rv) || bytesRead == 0) {
        return Nothing();
      }

      bytePtr += bytesRead;
      bytesLeft -= bytesRead;
    }

#ifdef DEBUG

    // Currently, all implementers of imgIEncoder report their exact size
    // through nsIInputStream::Available(), but let's explicitly state that we
    // rely on that behavior for the algorithm above.

    char dummy = 0;
    uint32_t bytesRead = 0;
    rv = imgStream->Read(&dummy, 1, &bytesRead);
    MOZ_ASSERT(NS_SUCCEEDED(rv) && bytesRead == 0);

#endif

    RecordedFrameData frameData;

    frameData.timeOffset() = recordedFrame->GetTimeStamp();
    frameData.length() = frameLength;

    recording.frames().AppendElement(std::move(frameData));

    // Now that we're done, release the frame so we can free up its memory
    recordedFrame = nullptr;
  }

  mRecordedFrames.Clear();

  recording.bytes() = ipc::BigBuffer(bytes);

  return Some(std::move(recording));
}

}  // namespace layers
}  // namespace mozilla
