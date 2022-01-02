/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AudioListener_h_
#define AudioListener_h_

#include "nsWrapperCache.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/Attributes.h"
#include "ThreeDPoint.h"
#include "AudioContext.h"
#include "PannerNode.h"
#include "WebAudioUtils.h"
#include "js/TypeDecls.h"
#include "mozilla/MemoryReporting.h"

namespace mozilla {

namespace dom {

class AudioListenerEngine final {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AudioListenerEngine)

  AudioListenerEngine();

  enum class AudioListenerParameter {
    POSITION,
    FRONT,  // unit length
    RIGHT   // unit length, orthogonal to FRONT
  };
  void RecvListenerEngineEvent(
      AudioListenerEngine::AudioListenerParameter aParameter,
      const ThreeDPoint& aValue);
  const ThreeDPoint& Position() const;
  const ThreeDPoint& FrontVector() const;
  const ThreeDPoint& RightVector() const;

 private:
  ~AudioListenerEngine() = default;

  ThreeDPoint mPosition;
  ThreeDPoint mFrontVector;
  ThreeDPoint mRightVector;
};

class AudioListener final : public nsWrapperCache {
 public:
  explicit AudioListener(AudioContext* aContext);

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(AudioListener)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(AudioListener)

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  AudioContext* GetParentObject() const { return mContext; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  void SetPosition(double aX, double aY, double aZ);
  void SetOrientation(double aX, double aY, double aZ, double aXUp, double aYUp,
                      double aZUp);

  AudioListenerEngine* Engine() { return mEngine.get(); }

 private:
  void SendListenerEngineEvent(
      AudioListenerEngine::AudioListenerParameter aParameter,
      const ThreeDPoint& aValue);

  ~AudioListener() = default;

 private:
  RefPtr<AudioContext> mContext;
  RefPtr<AudioListenerEngine> mEngine;
  ThreeDPoint mPosition;
  ThreeDPoint mFrontVector;
  ThreeDPoint mRightVector;
};

}  // namespace dom
}  // namespace mozilla

#endif
