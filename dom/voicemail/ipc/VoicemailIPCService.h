/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_voicemail_VoicemailIPCService_h
#define mozilla_dom_voicemail_VoicemailIPCService_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/voicemail/PVoicemailChild.h"
#include "nsIVoicemailService.h"

namespace mozilla {
namespace dom {
namespace voicemail {

class VoicemailIPCService final : public PVoicemailChild
                                , public nsIVoicemailService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIVOICEMAILSERVICE

  VoicemailIPCService();

  bool
  RecvNotifyInfoChanged(const uint32_t& aServiceId,
                        const nsString& aNumber,
                        const nsString& aDisplayName) override;

  bool
  RecvNotifyStatusChanged(const uint32_t& aServiceId,
                          const bool& aHasMessages,
                          const int32_t& aMessageCount,
                          const nsString& aNumber,
                          const nsString& aDisplayName) override;

  void
  ActorDestroy(ActorDestroyReason aWhy) override;

private:
  // final suppresses -Werror,-Wdelete-non-virtual-dtor
  ~VoicemailIPCService();

private:
  bool mActorDestroyed;
  nsTArray<nsCOMPtr<nsIVoicemailListener>> mListeners;
  nsTArray<nsCOMPtr<nsIVoicemailProvider>> mProviders;
};

} // namespace voicemail
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_voicemail_VoicemailIPCService_h
