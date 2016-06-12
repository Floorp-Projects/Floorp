/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_voicemail_VoicemailParent_h
#define mozilla_dom_voicemail_VoicemailParent_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/voicemail/PVoicemailParent.h"
#include "mozilla/dom/voicemail/VoicemailParent.h"
#include "nsIVoicemailService.h"
#include "nsString.h"

namespace mozilla {
namespace dom {
namespace voicemail {

class VoicemailParent final : public PVoicemailParent
                            , public nsIVoicemailListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIVOICEMAILLISTENER

  VoicemailParent() { MOZ_COUNT_CTOR(VoicemailParent); }

  bool
  Init();

  bool
  RecvGetAttributes(const uint32_t& aServiceId,
                    nsString* aNumber,
                    nsString* aDisplayName,
                    bool* aHasMessages,
                    int32_t* aMessageCount,
                    nsString* aReturnNumber,
                    nsString* aReturnMessage) override;

  void
  ActorDestroy(ActorDestroyReason aWhy) override;

private:
  // final suppresses -Werror,-Wdelete-non-virtual-dtor
  ~VoicemailParent() { MOZ_COUNT_DTOR(VoicemailParent); }

private:
  nsCOMPtr<nsIVoicemailService> mService;
};

} // namespace voicemail
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_voicemail_VoicemailParent_h
