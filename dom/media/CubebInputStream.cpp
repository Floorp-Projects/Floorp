/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "CubebInputStream.h"

#include "AudioSampleFormat.h"
#include "mozilla/Logging.h"

namespace mozilla {

extern mozilla::LazyLogModule gMediaTrackGraphLog;

#ifdef LOG_INTERNAL
#  undef LOG_INTERNAL
#endif  // LOG_INTERNAL
#define LOG_INTERNAL(level, msg, ...) \
  MOZ_LOG(gMediaTrackGraphLog, LogLevel::level, (msg, ##__VA_ARGS__))

#ifdef LOG
#  undef LOG
#endif  // LOG
#define LOG(msg, ...) LOG_INTERNAL(Debug, msg, ##__VA_ARGS__)

#ifdef LOGE
#  undef LOGE
#endif  // LOGE
#define LOGE(msg, ...) LOG_INTERNAL(Error, msg, ##__VA_ARGS__)

#define InvokeCubebWithLog(func, ...)                                          \
  ({                                                                           \
    int _retval;                                                               \
    _retval = InvokeCubeb(func, ##__VA_ARGS__);                                \
    if (_retval == CUBEB_OK) {                                                 \
      LOG("CubebInputStream %p: %s for stream %p was successful", this, #func, \
          mStream.get());                                                      \
    } else {                                                                   \
      LOGE("CubebInputStream %p: %s for stream %p was failed. Error %d", this, \
           #func, mStream.get(), _retval);                                     \
    }                                                                          \
    _retval;                                                                   \
  })

static cubeb_stream_params CreateStreamInitParams(uint32_t aChannels,
                                                  uint32_t aRate,
                                                  bool aIsVoice) {
  cubeb_stream_params params;
  params.format = CubebUtils::ToCubebFormat<AUDIO_OUTPUT_FORMAT>::value;
  params.rate = aRate;
  params.channels = aChannels;
  params.layout = CUBEB_LAYOUT_UNDEFINED;
  params.prefs = CubebUtils::GetDefaultStreamPrefs(CUBEB_DEVICE_TYPE_INPUT);

  if (aIsVoice) {
    params.prefs |= static_cast<cubeb_stream_prefs>(CUBEB_STREAM_PREF_VOICE);
  }

  return params;
}

void CubebInputStream::CubebDestroyPolicy::operator()(
    cubeb_stream* aStream) const {
  int r = cubeb_stream_register_device_changed_callback(aStream, nullptr);
  if (r == CUBEB_OK) {
    LOG("Unregister device changed callback for %p successfully", aStream);
  } else {
    LOGE("Fail to unregister device changed callback for %p. Error %d", aStream,
         r);
  }
  cubeb_stream_destroy(aStream);
}

/* static */
UniquePtr<CubebInputStream> CubebInputStream::Create(cubeb_devid aDeviceId,
                                                     uint32_t aChannels,
                                                     uint32_t aRate,
                                                     bool aIsVoice,
                                                     Listener* aListener) {
  if (!aListener) {
    LOGE("No available listener");
    return nullptr;
  }

  cubeb* context = CubebUtils::GetCubebContext();
  if (!context) {
    LOGE("No valid cubeb context");
    CubebUtils::ReportCubebStreamInitFailure(CubebUtils::GetFirstStream());
    return nullptr;
  }

  cubeb_stream_params params =
      CreateStreamInitParams(aChannels, aRate, aIsVoice);
  uint32_t latencyFrames = CubebUtils::GetCubebMTGLatencyInFrames(&params);

  cubeb_stream* cubebStream = nullptr;

  RefPtr<Listener> listener(aListener);
  if (int r = CubebUtils::CubebStreamInit(
          context, &cubebStream, "input-only stream", aDeviceId, &params,
          nullptr, nullptr, latencyFrames, DataCallback_s, StateCallback_s,
          listener.get());
      r != CUBEB_OK) {
    CubebUtils::ReportCubebStreamInitFailure(CubebUtils::GetFirstStream());
    LOGE("Fail to create a cubeb stream. Error %d", r);
    return nullptr;
  }

  UniquePtr<cubeb_stream, CubebDestroyPolicy> inputStream(cubebStream);

  LOG("Create a cubeb stream %p successfully", inputStream.get());

  UniquePtr<CubebInputStream> stream(
      new CubebInputStream(listener.forget(), std::move(inputStream)));
  stream->Init();
  return stream;
}

CubebInputStream::CubebInputStream(
    already_AddRefed<Listener>&& aListener,
    UniquePtr<cubeb_stream, CubebDestroyPolicy>&& aStream)
    : mListener(aListener), mStream(std::move(aStream)) {
  MOZ_ASSERT(mListener);
  MOZ_ASSERT(mStream);
}

void CubebInputStream::Init() {
  // cubeb_stream_register_device_changed_callback is only supported on macOS
  // platform and MockCubebfor now.
  InvokeCubebWithLog(cubeb_stream_register_device_changed_callback,
                     CubebInputStream::DeviceChangedCallback_s);
}

int CubebInputStream::Start() { return InvokeCubebWithLog(cubeb_stream_start); }

int CubebInputStream::Stop() { return InvokeCubebWithLog(cubeb_stream_stop); }

int CubebInputStream::Latency(uint32_t* aLatencyFrames) {
  return InvokeCubebWithLog(cubeb_stream_get_input_latency, aLatencyFrames);
}

template <typename Function, typename... Args>
int CubebInputStream::InvokeCubeb(Function aFunction, Args&&... aArgs) {
  MOZ_ASSERT(mStream);
  return aFunction(mStream.get(), std::forward<Args>(aArgs)...);
}

/* static */
long CubebInputStream::DataCallback_s(cubeb_stream* aStream, void* aUser,
                                      const void* aInputBuffer,
                                      void* aOutputBuffer, long aFrames) {
  MOZ_ASSERT(aUser);
  MOZ_ASSERT(aInputBuffer);
  MOZ_ASSERT(!aOutputBuffer);
  return static_cast<Listener*>(aUser)->DataCallback(aInputBuffer, aFrames);
}

/* static */
void CubebInputStream::StateCallback_s(cubeb_stream* aStream, void* aUser,
                                       cubeb_state aState) {
  MOZ_ASSERT(aUser);
  static_cast<Listener*>(aUser)->StateCallback(aState);
}

/* static */
void CubebInputStream::DeviceChangedCallback_s(void* aUser) {
  MOZ_ASSERT(aUser);
  static_cast<Listener*>(aUser)->DeviceChangedCallback();
}

#undef LOG_INTERNAL
#undef LOG
#undef LOGE

}  // namespace mozilla
