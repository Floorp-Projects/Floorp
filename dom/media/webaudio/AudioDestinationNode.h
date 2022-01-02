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
                       uint32_t aNumberOfChannels, uint32_t aLength);

  void DestroyMediaTrack() override;

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(AudioDestinationNode, AudioNode)
  NS_DECL_NSIAUDIOCHANNELAGENTCALLBACK

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  uint16_t NumberOfOutputs() const final { return 0; }

  uint32_t MaxChannelCount() const;
  void SetChannelCount(uint32_t aChannelCount, ErrorResult& aRv) override;

  void Init();
  void Close();

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

  const char* NodeType() const override { return "AudioDestinationNode"; }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override;
  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override;

  void NotifyDataAudibleStateChanged(bool aAudible);
  void ResolvePromise(AudioBuffer* aRenderedBuffer);

  unsigned long Length() {
    MOZ_ASSERT(mIsOffline);
    return mFramesToProduce;
  }

  void NotifyAudioContextStateChanged();

 protected:
  virtual ~AudioDestinationNode();

 private:
  // This would be created for non-offline audio context in order to receive
  // tab's mute/suspend/audio capture state change and update the audible state
  // to the tab.
  void CreateAndStartAudioChannelAgent();
  void DestroyAudioChannelAgentIfExists();
  RefPtr<AudioChannelAgent> mAudioChannelAgent;

  // These members are related to audio capturing. We would start capturing
  // audio if we're starting capturing audio from whole window, and MUST stop
  // capturing explicitly when we don't need to capture audio any more, because
  // we have to release the resource we allocated before.
  bool IsCapturingAudio() const;
  void StartAudioCapturingTrack();
  void StopAudioCapturingTrack();
  RefPtr<MediaInputPort> mCaptureTrackPort;

  // These members are used to determine if the destination node is actual
  // audible and `mFinalAudibleState` represents the final result.
  using AudibleChangedReasons = AudioChannelService::AudibleChangedReasons;
  using AudibleState = AudioChannelService::AudibleState;
  void UpdateFinalAudibleStateIfNeeded(AudibleChangedReasons aReason);
  bool IsAudible() const;
  bool mFinalAudibleState = false;
  bool mIsDataAudible = false;
  float mAudioChannelVolume = 1.0;

  // True if the audio channel disables the track for unvisited tab, and the
  // track will be enabled again when the tab gets first visited, or a user
  // presses the tab play icon.
  bool mAudioChannelDisabled = false;

  // When the destination node is audible, we would request a wakelock to
  // prevent computer from sleeping in order to keep audio playing.
  void CreateAudioWakeLockIfNeeded();
  void ReleaseAudioWakeLockIfExists();
  RefPtr<WakeLock> mWakeLock;

  SelfReference<AudioDestinationNode> mOfflineRenderingRef;
  uint32_t mFramesToProduce;

  RefPtr<Promise> mOfflineRenderingPromise;

  bool mIsOffline;

  // These varaibles are used to know how long AudioContext would become audible
  // since it was created.
  TimeStamp mCreatedTime;
  TimeDuration mDurationBeforeFirstTimeAudible;
};

}  // namespace dom
}  // namespace mozilla

#endif
