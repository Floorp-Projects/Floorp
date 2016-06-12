/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SpeakerManagerServicechild_h__
#define mozilla_dom_SpeakerManagerServicechild_h__

#include "nsISupports.h"
#include "SpeakerManagerService.h"

namespace mozilla {
namespace dom {
/* This class is used to do the IPC to enable/disable speaker status
   Also handle the application speaker competition problem
*/
class SpeakerManagerServiceChild : public SpeakerManagerService
{
public:
  /*
   * Return null or instance which has been created.
   */
  static SpeakerManagerService* GetSpeakerManagerService();
  /*
   * Return SpeakerManagerServiceChild instance.
   * If SpeakerManagerServiceChild is not exist, create and return new one.
   */
  static SpeakerManagerService* GetOrCreateSpeakerManagerService();
  static void Shutdown();
  virtual void ForceSpeaker(bool aEnable, bool aVisible) override;
  virtual bool GetSpeakerStatus() override;
  virtual void SetAudioChannelActive(bool aIsActive) override;
  virtual void Notify() override;
protected:
  SpeakerManagerServiceChild();
  virtual ~SpeakerManagerServiceChild();
};

} // namespace dom
} // namespace mozilla

#endif

