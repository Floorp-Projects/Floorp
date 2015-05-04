/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_voicemail_VoicemailStatus_h__
#define mozilla_dom_voicemail_VoicemailStatus_h__

#include "mozilla/Attributes.h"
#include "nsCOMPtr.h"
#include "nsIVoicemailService.h" // For nsIVoicemailProvider.
#include "nsString.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class VoicemailStatus final : public nsISupports
                            , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(VoicemailStatus)

  VoicemailStatus(nsISupports* aParent,
                  nsIVoicemailProvider* aProvider);

  nsISupports*
  GetParentObject() const { return mParent; }

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL interface

  uint32_t
  ServiceId() const;

  bool
  HasMessages() const;

  int32_t
  MessageCount() const;

  void
  GetReturnNumber(nsString& aReturnNumber) const;

  void
  GetReturnMessage(nsString& aReturnMessage) const;

private:
  // final suppresses -Werror,-Wdelete-non-virtual-dtor
  ~VoicemailStatus() {}

private:
  nsCOMPtr<nsISupports> mParent;
  nsCOMPtr<nsIVoicemailProvider> mProvider;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_voicemail_VoicemailStatus_h__
