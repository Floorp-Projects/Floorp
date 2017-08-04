/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioNotificationReceiver.h"
#include "AudioStream.h"          // for AudioStream
#include "mozilla/Logging.h"      // for LazyLogModule
#include "mozilla/StaticMutex.h"  // for StaticMutex
#include "mozilla/StaticPtr.h"    // for StaticAutoPtr
#include "nsAppRunner.h"          // for XRE_IsContentProcess
#include "nsTArray.h"             // for nsTArray

static mozilla::LazyLogModule sLogger("AudioNotificationReceiver");

#undef ANR_LOG
#define ANR_LOG(...) MOZ_LOG(sLogger, mozilla::LogLevel::Debug, (__VA_ARGS__))
#undef ANR_LOGW
#define ANR_LOGW(...) MOZ_LOG(sLogger, mozilla::LogLevel::Warning, (__VA_ARGS__))

namespace mozilla {
namespace audio {

/*
 * A list containing all clients subscribering the device-changed notifications.
 */
static StaticAutoPtr<nsTArray<AudioStream*>> sSubscribers;
static StaticMutex sMutex;

/*
 * AudioNotificationReceiver Implementation
 */
/* static */ void
AudioNotificationReceiver::Register(AudioStream* aAudioStream)
{
  MOZ_ASSERT(XRE_IsContentProcess());

  StaticMutexAutoLock lock(sMutex);
  if (!sSubscribers) {
    sSubscribers = new nsTArray<AudioStream*>();
  }
  sSubscribers->AppendElement(aAudioStream);

  ANR_LOG("The AudioStream: %p is registered successfully.", aAudioStream);
}

/* static */ void
AudioNotificationReceiver::Unregister(AudioStream* aAudioStream)
{
  MOZ_ASSERT(XRE_IsContentProcess());

  StaticMutexAutoLock lock(sMutex);
  MOZ_ASSERT(!sSubscribers->IsEmpty(), "No subscriber.");

  sSubscribers->RemoveElement(aAudioStream);
  if (sSubscribers->IsEmpty()) {
    // Clear the static pointer here to prevent memory leak.
    sSubscribers = nullptr;
  }

  ANR_LOG("The AudioStream: %p is unregistered successfully.", aAudioStream);
}

/* static */ void
AudioNotificationReceiver::NotifyDefaultDeviceChanged()
{
  MOZ_ASSERT(XRE_IsContentProcess());

  StaticMutexAutoLock lock(sMutex);

  // Do nothing when there is no AudioStream.
  if (!sSubscribers) {
    return;
  }

  for (AudioStream* stream : *sSubscribers) {
    ANR_LOG("Notify the AudioStream: %p that the default device has been changed.", stream);
    stream->ResetDefaultDevice();
  }
}

} // namespace audio
} // namespace mozilla
