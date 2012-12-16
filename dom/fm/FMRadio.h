/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_fm_radio_h__
#define mozilla_dom_fm_radio_h__

#include "nsCOMPtr.h"
#include "mozilla/HalTypes.h"
#include "nsDOMEventTargetHelper.h"
#include "nsIFMRadio.h"
#include "AudioChannelService.h"

#define NS_FMRADIO_CONTRACTID "@mozilla.org/fmradio;1"
// 9cb91834-78a9-4029-b644-7806173c5e2d
#define NS_FMRADIO_CID {0x9cb91834, 0x78a9, 0x4029, \
      {0xb6, 0x44, 0x78, 0x06, 0x17, 0x3c, 0x5e, 0x2d}}

namespace mozilla {
namespace dom {
namespace fm {

/* Header file */
class FMRadio : public nsDOMEventTargetHelper
              , public nsIFMRadio
              , public hal::FMRadioObserver
              , public hal::SwitchObserver
              , public nsIAudioChannelAgentCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIFMRADIO
  NS_DECL_NSIAUDIOCHANNELAGENTCALLBACK

  NS_FORWARD_NSIDOMEVENTTARGET(nsDOMEventTargetHelper::)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(
                                                   FMRadio,
                                                   nsDOMEventTargetHelper)
  FMRadio();
  virtual void Notify(const hal::FMRadioOperationInformation& info);
  virtual void Notify(const hal::SwitchEvent& aEvent);

private:
  ~FMRadio();

  hal::SwitchState mHeadphoneState;
  bool mHasInternalAntenna;
  bool mHidden;
  nsCOMPtr<nsIAudioChannelAgent> mAudioChannelAgent;
};

} // namespace fm
} // namespace dom
} // namespace mozilla
#endif

