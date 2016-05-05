/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_audio_channel_agent_h__
#define mozilla_dom_audio_channel_agent_h__

#include "nsIAudioChannelAgent.h"
#include "nsCycleCollectionParticipant.h"
#include "nsCOMPtr.h"
#include "nsWeakPtr.h"

#define NS_AUDIOCHANNELAGENT_CONTRACTID "@mozilla.org/audiochannelagent;1"
// f27688e2-3dd7-11e2-904e-10bf48d64bd4
#define NS_AUDIOCHANNELAGENT_CID {0xf27688e2, 0x3dd7, 0x11e2, \
      {0x90, 0x4e, 0x10, 0xbf, 0x48, 0xd6, 0x4b, 0xd4}}

class nsPIDOMWindowInner;
class nsPIDOMWindowOuter;

namespace mozilla {
namespace dom {

class AudioPlaybackConfig;

/* Header file */
class AudioChannelAgent : public nsIAudioChannelAgent
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIAUDIOCHANNELAGENT

  NS_DECL_CYCLE_COLLECTION_CLASS(AudioChannelAgent)

  AudioChannelAgent();

  void WindowVolumeChanged();
  void WindowSuspendChanged(nsSuspendedTypes aSuspend);
  void WindowAudioCaptureChanged(uint64_t aInnerWindowID, bool aCapture);

  nsPIDOMWindowOuter* Window() const
  {
    return mWindow;
  }

  uint64_t WindowID() const;
  uint64_t InnerWindowID() const;

private:
  virtual ~AudioChannelAgent();

  AudioPlaybackConfig GetMediaConfig();
  bool IsDisposableSuspend(nsSuspendedTypes aSuspend) const;

  // Returns mCallback if that's non-null, or otherwise tries to get an
  // nsIAudioChannelAgentCallback out of mWeakCallback.
  already_AddRefed<nsIAudioChannelAgentCallback> GetCallback();

  nsresult InitInternal(nsPIDOMWindowInner* aWindow, int32_t aAudioAgentType,
                        nsIAudioChannelAgentCallback* aCallback,
                        bool aUseWeakRef);

  void Shutdown();

  nsresult FindCorrectWindow(nsPIDOMWindowInner* aWindow);

  nsCOMPtr<nsPIDOMWindowOuter> mWindow;
  nsCOMPtr<nsIAudioChannelAgentCallback> mCallback;

  nsWeakPtr mWeakCallback;

  int32_t mAudioChannelType;
  uint64_t mInnerWindowID;
  bool mIsRegToService;
};

} // namespace dom
} // namespace mozilla


#endif
