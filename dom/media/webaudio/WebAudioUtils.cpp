/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebAudioUtils.h"
#include "AudioNodeTrack.h"
#include "blink/HRTFDatabaseLoader.h"

#include "nsContentUtils.h"
#include "nsIConsoleService.h"
#include "nsIScriptError.h"
#include "AudioEventTimeline.h"

#include "mozilla/SchedulerGroup.h"

namespace mozilla {

LazyLogModule gWebAudioAPILog("WebAudioAPI");

namespace dom {

void WebAudioUtils::ConvertAudioTimelineEventToTicks(AudioTimelineEvent& aEvent,
                                                     AudioNodeTrack* aDest) {
  aEvent.SetTimeInTicks(
      aDest->SecondsToNearestTrackTime(aEvent.Time<double>()));
  aEvent.mTimeConstant *= aDest->mSampleRate;
  aEvent.mDuration *= aDest->mSampleRate;
}

void WebAudioUtils::Shutdown() { WebCore::HRTFDatabaseLoader::shutdown(); }

int WebAudioUtils::SpeexResamplerProcess(SpeexResamplerState* aResampler,
                                         uint32_t aChannel, const float* aIn,
                                         uint32_t* aInLen, float* aOut,
                                         uint32_t* aOutLen) {
#ifdef MOZ_SAMPLE_TYPE_S16
  AutoTArray<AudioDataValue, WEBAUDIO_BLOCK_SIZE * 4> tmp1;
  AutoTArray<AudioDataValue, WEBAUDIO_BLOCK_SIZE * 4> tmp2;
  tmp1.SetLength(*aInLen);
  tmp2.SetLength(*aOutLen);
  ConvertAudioSamples(aIn, tmp1.Elements(), *aInLen);
  int result = speex_resampler_process_int(
      aResampler, aChannel, tmp1.Elements(), aInLen, tmp2.Elements(), aOutLen);
  ConvertAudioSamples(tmp2.Elements(), aOut, *aOutLen);
  return result;
#else
  return speex_resampler_process_float(aResampler, aChannel, aIn, aInLen, aOut,
                                       aOutLen);
#endif
}

int WebAudioUtils::SpeexResamplerProcess(SpeexResamplerState* aResampler,
                                         uint32_t aChannel, const int16_t* aIn,
                                         uint32_t* aInLen, float* aOut,
                                         uint32_t* aOutLen) {
  AutoTArray<AudioDataValue, WEBAUDIO_BLOCK_SIZE * 4> tmp;
#ifdef MOZ_SAMPLE_TYPE_S16
  tmp.SetLength(*aOutLen);
  int result = speex_resampler_process_int(aResampler, aChannel, aIn, aInLen,
                                           tmp.Elements(), aOutLen);
  ConvertAudioSamples(tmp.Elements(), aOut, *aOutLen);
  return result;
#else
  tmp.SetLength(*aInLen);
  ConvertAudioSamples(aIn, tmp.Elements(), *aInLen);
  int result = speex_resampler_process_float(
      aResampler, aChannel, tmp.Elements(), aInLen, aOut, aOutLen);
  return result;
#endif
}

int WebAudioUtils::SpeexResamplerProcess(SpeexResamplerState* aResampler,
                                         uint32_t aChannel, const int16_t* aIn,
                                         uint32_t* aInLen, int16_t* aOut,
                                         uint32_t* aOutLen) {
#ifdef MOZ_SAMPLE_TYPE_S16
  return speex_resampler_process_int(aResampler, aChannel, aIn, aInLen, aOut,
                                     aOutLen);
#else
  AutoTArray<AudioDataValue, WEBAUDIO_BLOCK_SIZE * 4> tmp1;
  AutoTArray<AudioDataValue, WEBAUDIO_BLOCK_SIZE * 4> tmp2;
  tmp1.SetLength(*aInLen);
  tmp2.SetLength(*aOutLen);
  ConvertAudioSamples(aIn, tmp1.Elements(), *aInLen);
  int result = speex_resampler_process_float(
      aResampler, aChannel, tmp1.Elements(), aInLen, tmp2.Elements(), aOutLen);
  ConvertAudioSamples(tmp2.Elements(), aOut, *aOutLen);
  return result;
#endif
}

void WebAudioUtils::LogToDeveloperConsole(uint64_t aWindowID,
                                          const char* aKey) {
  // This implementation is derived from dom/media/VideoUtils.cpp, but we
  // use a windowID so that the message is delivered to the developer console.
  // It is similar to ContentUtils::ReportToConsole, but also works off main
  // thread.
  if (!NS_IsMainThread()) {
    nsCOMPtr<nsIRunnable> task = NS_NewRunnableFunction(
        "dom::WebAudioUtils::LogToDeveloperConsole",
        [aWindowID, aKey] { LogToDeveloperConsole(aWindowID, aKey); });
    SchedulerGroup::Dispatch(TaskCategory::Other, task.forget());
    return;
  }

  nsCOMPtr<nsIConsoleService> console(
      do_GetService("@mozilla.org/consoleservice;1"));
  if (!console) {
    NS_WARNING("Failed to log message to console.");
    return;
  }

  nsAutoString spec;
  uint32_t aLineNumber, aColumnNumber;
  JSContext* cx = nsContentUtils::GetCurrentJSContext();
  if (cx) {
    nsJSUtils::GetCallingLocation(cx, spec, &aLineNumber, &aColumnNumber);
  }

  nsresult rv;
  nsCOMPtr<nsIScriptError> errorObject =
      do_CreateInstance(NS_SCRIPTERROR_CONTRACTID, &rv);
  if (!errorObject) {
    NS_WARNING("Failed to log message to console.");
    return;
  }

  nsAutoString result;
  rv = nsContentUtils::GetLocalizedString(nsContentUtils::eDOM_PROPERTIES, aKey,
                                          result);

  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to log message to console.");
    return;
  }

  errorObject->InitWithWindowID(result, spec, EmptyString(), aLineNumber,
                                aColumnNumber, nsIScriptError::warningFlag,
                                "Web Audio", aWindowID);
  console->LogMessage(errorObject);
}

}  // namespace dom
}  // namespace mozilla
