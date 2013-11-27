/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SpeakerManagerService_h__
#define mozilla_dom_SpeakerManagerService_h__

#include "nsAutoPtr.h"
#include "nsIObserver.h"
#include "nsTArray.h"
#include "SpeakerManager.h"
#include "nsIAudioManager.h"
#include "nsCheapSets.h"
#include "nsHashKeys.h"

namespace mozilla {
namespace dom {

class SpeakerManagerService : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static SpeakerManagerService* GetSpeakerManagerService();
  virtual void ForceSpeaker(bool aEnable, bool aVisible);
  virtual bool GetSpeakerStatus();
  virtual void SetAudioChannelActive(bool aIsActive);
  // Called by child
  void ForceSpeaker(bool enable, uint64_t aChildid);
  // Register the SpeakerManager to service for notify the speakerforcedchange event
  void RegisterSpeakerManager(SpeakerManager* aSpeakerManager)
  {
    mRegisteredSpeakerManagers.AppendElement(aSpeakerManager);
  }
  void UnRegisterSpeakerManager(SpeakerManager* aSpeakerManager)
  {
    mRegisteredSpeakerManagers.RemoveElement(aSpeakerManager);
  }
  /**
   * Shutdown the singleton.
   */
  static void Shutdown();

protected:
  SpeakerManagerService();

  virtual ~SpeakerManagerService();
  // Notify to UA if device speaker status changed
  virtual void Notify();

  void TuruOnSpeaker(bool aEnable);

  nsTArray<nsRefPtr<SpeakerManager> > mRegisteredSpeakerManagers;
  // Set for remember all the child speaker status
  nsCheapSet<nsUint64HashKey> mSpeakerStatusSet;
  // The Speaker status assign by UA
  bool mOrgSpeakerStatus;

  bool mVisible;
  // This is needed for IPC communication between
  // SpeakerManagerServiceChild and this class.
  friend class ContentParent;
  friend class ContentChild;
};

} // namespace dom
} // namespace mozilla

#endif
