/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MmsMessage_h
#define mozilla_dom_MmsMessage_h

#include "mozilla/dom/BindingDeclarations.h"
#include "nsWrapperCache.h"

class nsPIDOMWindowInner;

namespace mozilla {
namespace dom {

namespace mobilemessage {
class MmsMessageInternal;
} // namespace mobilemessage

struct MmsAttachment;
struct MmsDeliveryInfo;

class MmsMessage final : public nsISupports,
                         public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(MmsMessage)

  MmsMessage(nsPIDOMWindowInner* aWindow,
             mobilemessage::MmsMessageInternal* aMessage);

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
  GetDeliveryInfo(nsTArray<MmsDeliveryInfo>& aRetVal) const;

  void
  GetSender(nsString& aRetVal) const;

  void
  GetReceivers(nsTArray<nsString>& aRetVal) const;

  uint64_t
  Timestamp() const;

  uint64_t
  SentTimestamp() const;

  bool
  Read() const;

  void
  GetSubject(nsString& aRetVal) const;

  void
  GetSmil(nsString& aRetVal) const;

  void
  GetAttachments(nsTArray<MmsAttachment>& aRetVal) const;

  uint64_t
  ExpiryDate() const;

  bool
  ReadReportRequested() const;

private:
  // Don't try to use the default constructor.
  MmsMessage() = delete;

  ~MmsMessage();

  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  RefPtr<mobilemessage::MmsMessageInternal> mMessage;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_MmsMessage_h
