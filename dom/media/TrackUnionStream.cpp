/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaStreamGraphImpl.h"
#include "MediaStreamListener.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Unused.h"

#include "AudioSegment.h"
#include "VideoSegment.h"
#include "nsContentUtils.h"
#include "nsIAppShell.h"
#include "nsIObserver.h"
#include "nsPrintfCString.h"
#include "nsServiceManagerUtils.h"
#include "nsWidgetsCID.h"
#include "prerror.h"
#include "mozilla/Logging.h"
#include "mozilla/Attributes.h"
#include "TrackUnionStream.h"
#include "ImageContainer.h"
#include "AudioChannelService.h"
#include "AudioNodeEngine.h"
#include "AudioNodeStream.h"
#include "AudioNodeExternalInputStream.h"
#include "webaudio/MediaStreamAudioDestinationNode.h"
#include <algorithm>
#include "DOMMediaStream.h"
#include "GeckoProfiler.h"

using namespace mozilla::layers;
using namespace mozilla::dom;
using namespace mozilla::gfx;

namespace mozilla {

#ifdef STREAM_LOG
#  undef STREAM_LOG
#endif

LazyLogModule gTrackUnionStreamLog("TrackUnionStream");
#define STREAM_LOG(type, msg) MOZ_LOG(gTrackUnionStreamLog, type, msg)

TrackUnionStream::TrackUnionStream(TrackRate aSampleRate,
                                   MediaSegment::Type aType)
    : ProcessedMediaStream(
          aSampleRate, aType,
          aType == MediaSegment::AUDIO
              ? static_cast<MediaSegment*>(new AudioSegment())
              : static_cast<MediaSegment*>(new VideoSegment())) {}

void TrackUnionStream::AddInput(MediaInputPort* aPort) {
  SetInput(aPort);
  ProcessedMediaStream::AddInput(aPort);
}

void TrackUnionStream::RemoveInput(MediaInputPort* aPort) {
  STREAM_LOG(LogLevel::Debug,
             ("TrackUnionStream %p removing input %p", this, aPort));
  MOZ_ASSERT(aPort == mInputPort);
  nsTArray<RefPtr<DirectMediaStreamTrackListener>> listeners(
      mOwnedDirectListeners);
  for (const auto& listener : listeners) {
    // Remove listeners while the entry still exists.
    RemoveDirectListenerImpl(listener);
  }
  mInputPort = nullptr;
  ProcessedMediaStream::RemoveInput(aPort);
}

void TrackUnionStream::SetInput(MediaInputPort* aPort) {
  MOZ_ASSERT(aPort);
  MOZ_ASSERT(aPort->GetSource());
  MOZ_ASSERT(aPort->GetSource()->GetData());
  MOZ_ASSERT(!mInputPort);
  mInputPort = aPort;

  for (const auto& listener : mOwnedDirectListeners) {
    MediaStream* source = mInputPort->GetSource();
    STREAM_LOG(LogLevel::Debug, ("TrackUnionStream %p adding direct listener "
                                 "%p. Forwarding to input stream %p.",
                                 this, listener.get(), aPort->GetSource()));
    source->AddDirectListenerImpl(do_AddRef(listener));
  }
}

void TrackUnionStream::ProcessInputImpl(MediaStream* aSource,
                                        MediaSegment* aSegment, GraphTime aFrom,
                                        GraphTime aTo, uint32_t aFlags) {
  GraphTime next;
  for (GraphTime t = aFrom; t < aTo; t = next) {
    MediaInputPort::InputInterval interval =
        MediaInputPort::GetNextInputInterval(mInputPort, t);
    interval.mEnd = std::min(interval.mEnd, aTo);

    const bool inputEnded =
        !aSource ||
        (aSource->Ended() &&
         aSource->GetEnd() <=
             aSource->GraphTimeToStreamTimeWithBlocking(interval.mEnd));

    StreamTime ticks = interval.mEnd - interval.mStart;
    next = interval.mEnd;

    if (interval.mStart >= interval.mEnd) {
      break;
    }

    if (inputEnded) {
      if (mAutoend && (aFlags & ALLOW_END)) {
        mEnded = true;
        break;
      }
      aSegment->AppendNullData(ticks);
      STREAM_LOG(LogLevel::Verbose, ("TrackUnionStream %p appending %lld ticks "
                                     "of null data (ended input)",
                                     this, (long long)ticks));
    } else if (interval.mInputIsBlocked) {
      aSegment->AppendNullData(ticks);
      STREAM_LOG(LogLevel::Verbose, ("TrackUnionStream %p appending %lld ticks "
                                     "of null data (blocked input)",
                                     this, (long long)ticks));
    } else if (InMutedCycle()) {
      aSegment->AppendNullData(ticks);
    } else if (aSource->IsSuspended()) {
      aSegment->AppendNullData(aTo - aFrom);
    } else {
      MOZ_ASSERT(GetEnd() == GraphTimeToStreamTimeWithBlocking(interval.mStart),
                 "Samples missing");
      StreamTime inputStart =
          aSource->GraphTimeToStreamTimeWithBlocking(interval.mStart);
      StreamTime inputEnd =
          aSource->GraphTimeToStreamTimeWithBlocking(interval.mEnd);
      aSegment->AppendSlice(*aSource->GetData(), inputStart, inputEnd);
    }
    ApplyTrackDisabling(aSegment);
    for (const auto& listener : mTrackListeners) {
      listener->NotifyQueuedChanges(Graph(), GetEnd(), *aSegment);
    }
    mSegment->AppendFrom(aSegment);
  }
}

void TrackUnionStream::ProcessInput(GraphTime aFrom, GraphTime aTo,
                                    uint32_t aFlags) {
  TRACE_AUDIO_CALLBACK_COMMENT("TrackUnionStream %p", this);
  if (mEnded) {
    return;
  }

  MediaInputPort* input = mInputPort;
  MediaStream* source = input ? input->GetSource() : nullptr;
  if (mType == MediaSegment::AUDIO) {
    AudioSegment audio;
    ProcessInputImpl(source, &audio, aFrom, aTo, aFlags);
  } else if (mType == MediaSegment::VIDEO) {
    VideoSegment video;
    ProcessInputImpl(source, &video, aFrom, aTo, aFlags);
  } else {
    MOZ_CRASH("Unknown segment type");
  }
}

void TrackUnionStream::SetEnabledImpl(DisabledTrackMode aMode) {
  bool enabled = aMode == DisabledTrackMode::ENABLED;
  STREAM_LOG(LogLevel::Info, ("TrackUnionStream %p was explicitly %s", this,
                              enabled ? "enabled" : "disabled"));
  for (DirectMediaStreamTrackListener* listener : mOwnedDirectListeners) {
    DisabledTrackMode oldMode = mDisabledMode;
    bool oldEnabled = oldMode == DisabledTrackMode::ENABLED;
    if (!oldEnabled && enabled) {
      STREAM_LOG(LogLevel::Debug, ("TrackUnionStream %p setting "
                                   "direct listener enabled",
                                   this));
      listener->DecreaseDisabled(oldMode);
    } else if (oldEnabled && !enabled) {
      STREAM_LOG(LogLevel::Debug, ("TrackUnionStream %p setting "
                                   "direct listener disabled",
                                   this));
      listener->IncreaseDisabled(aMode);
    }
  }
  MediaStream::SetEnabledImpl(aMode);
}

void TrackUnionStream::AddDirectListenerImpl(
    already_AddRefed<DirectMediaStreamTrackListener> aListener) {
  RefPtr<DirectMediaStreamTrackListener> listener = aListener;
  mOwnedDirectListeners.AppendElement(listener);

  DisabledTrackMode currentMode = mDisabledMode;
  if (currentMode != DisabledTrackMode::ENABLED) {
    listener->IncreaseDisabled(currentMode);
  }

  if (mInputPort) {
    MediaStream* source = mInputPort->GetSource();
    STREAM_LOG(LogLevel::Debug, ("TrackUnionStream %p adding direct listener "
                                 "%p. Forwarding to input stream %p.",
                                 this, listener.get(), source));
    source->AddDirectListenerImpl(listener.forget());
  }
}

void TrackUnionStream::RemoveDirectListenerImpl(
    DirectMediaStreamTrackListener* aListener) {
  for (size_t i = 0; i < mOwnedDirectListeners.Length(); ++i) {
    if (mOwnedDirectListeners[i] == aListener) {
      STREAM_LOG(
          LogLevel::Debug,
          ("TrackUnionStream %p removing direct listener %p", this, aListener));
      DisabledTrackMode currentMode = mDisabledMode;
      if (currentMode != DisabledTrackMode::ENABLED) {
        // Reset the listener's state.
        aListener->DecreaseDisabled(currentMode);
      }
      mOwnedDirectListeners.RemoveElementAt(i);
      break;
    }
  }
  if (mInputPort) {
    // Forward to the input
    MediaStream* source = mInputPort->GetSource();
    source->RemoveDirectListenerImpl(aListener);
  }
}

void TrackUnionStream::RemoveAllDirectListenersImpl() {
  nsTArray<RefPtr<DirectMediaStreamTrackListener>> listeners(
      mOwnedDirectListeners);
  for (const auto& listener : listeners) {
    RemoveDirectListenerImpl(listener);
  }
  MOZ_DIAGNOSTIC_ASSERT(mOwnedDirectListeners.IsEmpty());
}

}  // namespace mozilla
