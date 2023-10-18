/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ForwardedInputTrack.h"

#include <algorithm>
#include "AudioChannelService.h"
#include "AudioNodeEngine.h"
#include "AudioNodeExternalInputTrack.h"
#include "AudioNodeTrack.h"
#include "AudioSegment.h"
#include "DOMMediaStream.h"
#include "GeckoProfiler.h"
#include "ImageContainer.h"
#include "MediaTrackGraph.h"
#include "mozilla/Attributes.h"
#include "mozilla/Logging.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Unused.h"
#include "nsContentUtils.h"
#include "nsPrintfCString.h"
#include "nsServiceManagerUtils.h"
#include "nsWidgetsCID.h"
#include "prerror.h"
#include "Tracing.h"
#include "VideoSegment.h"
#include "webaudio/MediaStreamAudioDestinationNode.h"

using namespace mozilla::layers;
using namespace mozilla::dom;
using namespace mozilla::gfx;

namespace mozilla {

#ifdef TRACK_LOG
#  undef TRACK_LOG
#endif

LazyLogModule gForwardedInputTrackLog("ForwardedInputTrack");
#define TRACK_LOG(type, msg) MOZ_LOG(gForwardedInputTrackLog, type, msg)

ForwardedInputTrack::ForwardedInputTrack(TrackRate aSampleRate,
                                         MediaSegment::Type aType)
    : ProcessedMediaTrack(
          aSampleRate, aType,
          aType == MediaSegment::AUDIO
              ? static_cast<MediaSegment*>(new AudioSegment())
              : static_cast<MediaSegment*>(new VideoSegment())) {}

void ForwardedInputTrack::AddInput(MediaInputPort* aPort) {
  SetInput(aPort);
  ProcessedMediaTrack::AddInput(aPort);
}

void ForwardedInputTrack::RemoveInput(MediaInputPort* aPort) {
  TRACK_LOG(LogLevel::Debug,
            ("ForwardedInputTrack %p removing input %p", this, aPort));
  MOZ_ASSERT(aPort == mInputPort);

  for (const auto& listener : mOwnedDirectListeners) {
    MediaTrack* source = mInputPort->GetSource();
    TRACK_LOG(LogLevel::Debug,
              ("ForwardedInputTrack %p removing direct listener "
               "%p. Forwarding to input track %p.",
               this, listener.get(), aPort->GetSource()));
    source->RemoveDirectListenerImpl(listener);
  }

  DisabledTrackMode oldMode = CombinedDisabledMode();
  mInputDisabledMode = DisabledTrackMode::ENABLED;
  NotifyIfDisabledModeChangedFrom(oldMode);

  mInputPort = nullptr;
  ProcessedMediaTrack::RemoveInput(aPort);
}

void ForwardedInputTrack::SetInput(MediaInputPort* aPort) {
  MOZ_ASSERT(aPort);
  MOZ_ASSERT(aPort->GetSource());
  MOZ_ASSERT(aPort->GetSource()->GetData());
  MOZ_ASSERT(!mInputPort);
  MOZ_ASSERT(mInputDisabledMode == DisabledTrackMode::ENABLED);

  mInputPort = aPort;

  for (const auto& listener : mOwnedDirectListeners) {
    MediaTrack* source = mInputPort->GetSource();
    TRACK_LOG(LogLevel::Debug, ("ForwardedInputTrack %p adding direct listener "
                                "%p. Forwarding to input track %p.",
                                this, listener.get(), aPort->GetSource()));
    source->AddDirectListenerImpl(do_AddRef(listener));
  }

  DisabledTrackMode oldMode = CombinedDisabledMode();
  mInputDisabledMode = mInputPort->GetSource()->CombinedDisabledMode();
  NotifyIfDisabledModeChangedFrom(oldMode);
}

void ForwardedInputTrack::ProcessInputImpl(MediaTrack* aSource,
                                           MediaSegment* aSegment,
                                           GraphTime aFrom, GraphTime aTo,
                                           uint32_t aFlags) {
  GraphTime next;
  for (GraphTime t = aFrom; t < aTo; t = next) {
    MediaInputPort::InputInterval interval =
        MediaInputPort::GetNextInputInterval(mInputPort, t);
    interval.mEnd = std::min(interval.mEnd, aTo);

    const bool inputEnded =
        !aSource ||
        (aSource->Ended() &&
         aSource->GetEnd() <=
             aSource->GraphTimeToTrackTimeWithBlocking(interval.mEnd));

    TrackTime ticks = interval.mEnd - interval.mStart;
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
      TRACK_LOG(LogLevel::Verbose,
                ("ForwardedInputTrack %p appending %lld ticks "
                 "of null data (ended input)",
                 this, (long long)ticks));
    } else if (interval.mInputIsBlocked) {
      aSegment->AppendNullData(ticks);
      TRACK_LOG(LogLevel::Verbose,
                ("ForwardedInputTrack %p appending %lld ticks "
                 "of null data (blocked input)",
                 this, (long long)ticks));
    } else if (InMutedCycle()) {
      aSegment->AppendNullData(ticks);
    } else if (aSource->IsSuspended()) {
      aSegment->AppendNullData(ticks);
    } else {
      MOZ_ASSERT(GetEnd() == GraphTimeToTrackTimeWithBlocking(interval.mStart),
                 "Samples missing");
      TrackTime inputStart =
          aSource->GraphTimeToTrackTimeWithBlocking(interval.mStart);
      TrackTime inputEnd =
          aSource->GraphTimeToTrackTimeWithBlocking(interval.mEnd);
      aSegment->AppendSlice(*aSource->GetData(), inputStart, inputEnd);
    }
    ApplyTrackDisabling(aSegment);
    for (const auto& listener : mTrackListeners) {
      listener->NotifyQueuedChanges(Graph(), GetEnd(), *aSegment);
    }
    mSegment->AppendFrom(aSegment);
  }
}

void ForwardedInputTrack::ProcessInput(GraphTime aFrom, GraphTime aTo,
                                       uint32_t aFlags) {
  TRACE_COMMENT("ForwardedInputTrack::ProcessInput", "ForwardedInputTrack %p",
                this);
  if (mEnded) {
    return;
  }

  MediaInputPort* input = mInputPort;
  MediaTrack* source = input ? input->GetSource() : nullptr;
  if (mType == MediaSegment::AUDIO) {
    AudioSegment audio;
    ProcessInputImpl(source, &audio, aFrom, aTo, aFlags);
  } else if (mType == MediaSegment::VIDEO) {
    VideoSegment video;
    ProcessInputImpl(source, &video, aFrom, aTo, aFlags);
  } else {
    MOZ_CRASH("Unknown segment type");
  }

  if (mEnded) {
    RemoveAllDirectListenersImpl();
  }
}

DisabledTrackMode ForwardedInputTrack::CombinedDisabledMode() const {
  if (mDisabledMode == DisabledTrackMode::SILENCE_BLACK ||
      mInputDisabledMode == DisabledTrackMode::SILENCE_BLACK) {
    return DisabledTrackMode::SILENCE_BLACK;
  }
  if (mDisabledMode == DisabledTrackMode::SILENCE_FREEZE ||
      mInputDisabledMode == DisabledTrackMode::SILENCE_FREEZE) {
    return DisabledTrackMode::SILENCE_FREEZE;
  }
  return DisabledTrackMode::ENABLED;
}

void ForwardedInputTrack::SetDisabledTrackModeImpl(DisabledTrackMode aMode) {
  bool enabled = aMode == DisabledTrackMode::ENABLED;
  TRACK_LOG(LogLevel::Info, ("ForwardedInputTrack %p was explicitly %s", this,
                             enabled ? "enabled" : "disabled"));
  for (DirectMediaTrackListener* listener : mOwnedDirectListeners) {
    DisabledTrackMode oldMode = mDisabledMode;
    bool oldEnabled = oldMode == DisabledTrackMode::ENABLED;
    if (!oldEnabled && enabled) {
      TRACK_LOG(LogLevel::Debug, ("ForwardedInputTrack %p setting "
                                  "direct listener enabled",
                                  this));
      listener->DecreaseDisabled(oldMode);
    } else if (oldEnabled && !enabled) {
      TRACK_LOG(LogLevel::Debug, ("ForwardedInputTrack %p setting "
                                  "direct listener disabled",
                                  this));
      listener->IncreaseDisabled(aMode);
    }
  }
  MediaTrack::SetDisabledTrackModeImpl(aMode);
}

void ForwardedInputTrack::OnInputDisabledModeChanged(
    DisabledTrackMode aInputMode) {
  MOZ_ASSERT(mInputs.Length() == 1);
  MOZ_ASSERT(mInputs[0]->GetSource());
  DisabledTrackMode oldMode = CombinedDisabledMode();
  if (mInputDisabledMode == DisabledTrackMode::SILENCE_BLACK &&
      aInputMode == DisabledTrackMode::SILENCE_FREEZE) {
    // Don't allow demoting from SILENCE_BLACK to SILENCE_FREEZE. Frames will
    // remain black so we shouldn't notify that the track got enabled.
    aInputMode = DisabledTrackMode::SILENCE_BLACK;
  }
  mInputDisabledMode = aInputMode;
  NotifyIfDisabledModeChangedFrom(oldMode);
}

uint32_t ForwardedInputTrack::NumberOfChannels() const {
  MOZ_DIAGNOSTIC_ASSERT(mSegment->GetType() == MediaSegment::AUDIO);
  if (!mInputPort || !mInputPort->GetSource()) {
    return GetData<AudioSegment>()->MaxChannelCount();
  }
  return mInputPort->GetSource()->NumberOfChannels();
}

void ForwardedInputTrack::AddDirectListenerImpl(
    already_AddRefed<DirectMediaTrackListener> aListener) {
  RefPtr<DirectMediaTrackListener> listener = aListener;
  mOwnedDirectListeners.AppendElement(listener);

  DisabledTrackMode currentMode = mDisabledMode;
  if (currentMode != DisabledTrackMode::ENABLED) {
    listener->IncreaseDisabled(currentMode);
  }

  if (mInputPort) {
    MediaTrack* source = mInputPort->GetSource();
    TRACK_LOG(LogLevel::Debug, ("ForwardedInputTrack %p adding direct listener "
                                "%p. Forwarding to input track %p.",
                                this, listener.get(), source));
    source->AddDirectListenerImpl(listener.forget());
  }
}

void ForwardedInputTrack::RemoveDirectListenerImpl(
    DirectMediaTrackListener* aListener) {
  for (size_t i = 0; i < mOwnedDirectListeners.Length(); ++i) {
    if (mOwnedDirectListeners[i] == aListener) {
      TRACK_LOG(LogLevel::Debug,
                ("ForwardedInputTrack %p removing direct listener %p", this,
                 aListener));
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
    MediaTrack* source = mInputPort->GetSource();
    source->RemoveDirectListenerImpl(aListener);
  }
}

void ForwardedInputTrack::RemoveAllDirectListenersImpl() {
  for (const auto& listener : mOwnedDirectListeners.Clone()) {
    RemoveDirectListenerImpl(listener);
  }
  MOZ_DIAGNOSTIC_ASSERT(mOwnedDirectListeners.IsEmpty());
}

}  // namespace mozilla
