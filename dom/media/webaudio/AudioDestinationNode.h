/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AudioDestinationNode_h_
#define AudioDestinationNode_h_

#include "AudioChannelService.h"
#include "AudioNode.h"
#include "AudioChannelAgent.h"
#include "mozilla/TimeStamp.h"

namespace mozilla {
namespace dom {

class AudioContext;
class WakeLock;

class AudioDestinationNode final : public AudioNode,
                                   public nsIAudioChannelAgentCallback,
                                   public MainThreadMediaTrackListener {
 public:
  // This node type knows what MediaTrackGraph to use based on
  // whether it's in offline mode.
  AudioDestinationNode(AudioContext* aContext, bool aIsOffline,
                       bool aAllowedToStart, uint32_t aNumberOfChannels,
                       uint32_t aLength);

  void DestroyMediaTrack() override;

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(AudioDestinationNode, AudioNode)
  NS_DECL_NSIAUDIOCHANNELAGENTCALLBACK

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  uint16_t NumberOfOutputs() const final { return 0; }

  uint32_t MaxChannelCount() const;
  void SetChannelCount(uint32_t aChannelCount, ErrorResult& aRv) override;

  // Returns the track or null after unlink.
  AudioNodeTrack* Track();

  void Mute();
  void Unmute();

  void Suspend();
  void Resume();

  void StartRendering(Promise* aPromise);

  void OfflineShutdown();

  void NotifyMainThreadTrackEnded() override;
  void FireOfflineCompletionEvent();

  nsresult CreateAudioChannelAgent();
  void DestroyAudioChannelAgent();

  const char* NodeType() const override { return "AudioDestinationNode"; }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override;
  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override;

  void NotifyAudibleStateChanged(bool aAudible);
  void ResolvePromise(AudioBuffer* aRenderedBuffer);

  unsigned long Length() {
    MOZ_ASSERT(mIsOffline);
    return mFramesToProduce;
  }

 protected:
  virtual ~AudioDestinationNode();

 private:
  // These function are related to audio capturing. We would start capturing
  // audio if we're starting capturing audio from whole window, and MUST stop
  // capturing explicitly when we don't need to capture audio any more, because
  // we have to release the resource we allocated before.
  bool IsCapturingAudio() const;
  void StartAudioCapturingTrack();
  void StopAudioCapturingTrack();
  void CreateAudioWakeLockIfNeeded();
  void ReleaseAudioWakeLockIfExists();

  SelfReference<AudioDestinationNode> mOfflineRenderingRef;
  uint32_t mFramesToProduce;

  RefPtr<AudioChannelAgent> mAudioChannelAgent;
  RefPtr<MediaInputPort> mCaptureTrackPort;

  RefPtr<Promise> mOfflineRenderingPromise;
  RefPtr<WakeLock> mWakeLock;

  bool mIsOffline;
  bool mAudioChannelSuspended;

  AudioChannelService::AudibleState mAudible;

  // These varaibles are used to know how long AudioContext would become audible
  // since it was created.
  TimeStamp mCreatedTime;
  TimeDuration mDurationBeforeFirstTimeAudible;
};

}  // namespace dom
}  // namespace mozilla

#endif
