/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_CUBEBINPUTSTREAM_H_
#define DOM_MEDIA_CUBEBINPUTSTREAM_H_

#include "CubebUtils.h"
#include "mozilla/Maybe.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "nsISupportsImpl.h"

namespace mozilla {

// A light-weight wrapper to operate the C style Cubeb APIs for an input-only
// audio stream in a C++-friendly way.
// Limitation: Do not call these APIs in an audio callback thread. Otherwise we
// may get a deadlock.
class CubebInputStream final {
 public:
  ~CubebInputStream() = default;

  class Listener {
   public:
    NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING;

    // This will be fired on audio callback thread.
    virtual long DataCallback(const void* aBuffer, long aFrames) = 0;
    // This can be fired on any thread.
    virtual void StateCallback(cubeb_state aState) = 0;
    // This can be fired on any thread.
    virtual void DeviceChangedCallback() = 0;

   protected:
    Listener() = default;
    virtual ~Listener() = default;
  };

  // Return a non-null pointer if the stream has been initialized
  // successfully. Otherwise return a null pointer.
  static UniquePtr<CubebInputStream> Create(cubeb_devid aDeviceId,
                                            uint32_t aChannels, uint32_t aRate,
                                            bool aIsVoice, Listener* aListener);

  // Start producing audio data.
  int Start();

  // Stop producing audio data.
  int Stop();

  // Gets the approximate stream latency in frames.
  int Latency(uint32_t* aLatencyFrames);

 private:
  struct CubebDestroyPolicy {
    void operator()(cubeb_stream* aStream) const;
  };
  CubebInputStream(already_AddRefed<Listener>&& aListener,
                   UniquePtr<cubeb_stream, CubebDestroyPolicy>&& aStream);

  void Init();

  template <typename Function, typename... Args>
  int InvokeCubeb(Function aFunction, Args&&... aArgs);

  // Static wrapper function cubeb callbacks.
  static long DataCallback_s(cubeb_stream* aStream, void* aUser,
                             const void* aInputBuffer, void* aOutputBuffer,
                             long aFrames);
  static void StateCallback_s(cubeb_stream* aStream, void* aUser,
                              cubeb_state aState);
  static void DeviceChangedCallback_s(void* aUser);

  // mListener must outlive the life time of the mStream.
  const RefPtr<Listener> mListener;
  const UniquePtr<cubeb_stream, CubebDestroyPolicy> mStream;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_CUBEBINPUTSTREAM_H_
