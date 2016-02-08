/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SmsMessage_h
#define mozilla_dom_SmsMessage_h

#include "mozilla/dom/BindingDeclarations.h"
#include "nsWrapperCache.h"

class nsPIDOMWindowInner;

namespace mozilla {
namespace dom {

namespace mobilemessage {
class SmsMessageInternal;
} // namespace mobilemessage

/**
 * Each instance of this class provides the DOM-level representation of
 * a SmsMessage object to bind it to a window being exposed to.
 */
class SmsMessage final : public nsISupports,
                         public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(SmsMessage)

  SmsMessage(nsPIDOMWindowInner* aWindow,
             mobilemessage::SmsMessageInternal* aMessage);

public:
  nsPIDOMWindowInner*
  GetParentObject() const
  {
    return mWindow;
  }

  virtual JSObject*
  WrapObject(JSContext* aCx,
             JS::Handle<JSObject*> aGivenProto) override;

  void
  GetType(nsString& aRetVal) const;

  int32_t
  Id() const;

  uint64_t
  ThreadId() const;

  void
  GetIccId(nsString& aRetVal) const;

  void
  GetDelivery(nsString& aRetVal) const;

  void
  GetDeliveryStatus(nsString& aRetVal) const;

  void
  GetSender(nsString& aRetVal) const;

  void
  GetReceiver(nsString& aRetVal) const;

  void
  GetBody(nsString& aRetVal) const;

  void
  GetMessageClass(nsString& aRetVal) const;

  uint64_t
  Timestamp() const;

  uint64_t
  SentTimestamp() const;

  uint64_t
  DeliveryTimestamp() const;

  bool
  Read() const;

private:
  // Don't try to use the default constructor.
  SmsMessage() = delete;

  ~SmsMessage();

  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  RefPtr<mobilemessage::SmsMessageInternal> mMessage;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SmsMessage_h
