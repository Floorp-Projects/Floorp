/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SpeakerManagerServicechild_h__
#define mozilla_dom_SpeakerManagerServicechild_h__

#include "nsAutoPtr.h"
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
  static SpeakerManagerService* GetSpeakerManagerService();
  static void Shutdown();
  virtual void ForceSpeaker(bool aEnable, bool aVisible) MOZ_OVERRIDE;
  virtual bool GetSpeakerStatus() MOZ_OVERRIDE;
  virtual void SetAudioChannelActive(bool aIsActive) MOZ_OVERRIDE;
  virtual void Notify() MOZ_OVERRIDE;
protected:
  SpeakerManagerServiceChild();
  virtual ~SpeakerManagerServiceChild();
};

} // namespace dom
} // namespace mozilla

#endif

