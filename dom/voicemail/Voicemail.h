/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_voicemail_voicemail_h__
#define mozilla_dom_voicemail_voicemail_h__

#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/ErrorResult.h"
#include "nsIVoicemailService.h"

class JSObject;
struct JSContext;

class nsPIDOMWindow;

namespace mozilla {
namespace dom {

class VoicemailStatus;

class Voicemail final : public DOMEventTargetHelper,
                        private nsIVoicemailListener
{
  /**
   * Class Voicemail doesn't actually expose nsIVoicemailListener. Instead, it
   * owns an nsIVoicemailListener derived instance mListener and passes it to
   * nsIVoicemailService. The onreceived events are first delivered to
   * mListener and then forwarded to its owner, Voicemail. See also bug 775997
   * comment #51.
   */
  class Listener;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIVOICEMAILLISTENER

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(Voicemail,
                                           DOMEventTargetHelper)

  static already_AddRefed<Voicemail>
  Create(nsPIDOMWindow* aOwner,
         ErrorResult& aRv);

  void
  Shutdown();

  nsPIDOMWindow*
  GetParentObject() const
  {
    return GetOwner();
  }

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  already_AddRefed<VoicemailStatus>
  GetStatus(const Optional<uint32_t>& aServiceId,
            ErrorResult& aRv);

  void
  GetNumber(const Optional<uint32_t>& aServiceId,
            nsString& aNumber,
            ErrorResult& aRv) const;

  void
  GetDisplayName(const Optional<uint32_t>& aServiceId,
                 nsString& aDisplayName,
                 ErrorResult& aRv) const;

  IMPL_EVENT_HANDLER(statuschanged)

private:
  Voicemail(nsPIDOMWindow* aWindow,
            nsIVoicemailService* aService);

  // final suppresses -Werror,-Wdelete-non-virtual-dtor
  ~Voicemail();

private:
  nsCOMPtr<nsIVoicemailService> mService;
  nsRefPtr<Listener> mListener;

  // |mStatuses| keeps all instantiated VoicemailStatus objects as well as the
  // empty slots for not interested ones. The length of |mStatuses| is decided
  // in the constructor and is never changed ever since.
  nsAutoTArray<nsRefPtr<VoicemailStatus>, 1> mStatuses;

  // Return a nsIVoicemailProvider instance based on the requests from external
  // components. Return nullptr if aOptionalServiceId contains an invalid
  // service id or the default one is just not available.
  already_AddRefed<nsIVoicemailProvider>
  GetItemByServiceId(const Optional<uint32_t>& aOptionalServiceId,
                     uint32_t& aActualServiceId) const;

  // Request for a valid VoicemailStatus object based on given service id and
  // provider. It's the callee's responsibility to ensure the validity of the
  // two parameters.
  already_AddRefed<VoicemailStatus>
  GetOrCreateStatus(uint32_t aServiceId,
                    nsIVoicemailProvider* aProvider);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_voicemail_voicemail_h__
