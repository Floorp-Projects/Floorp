/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mobilemessage_MobileMessageManager_h
#define mozilla_dom_mobilemessage_MobileMessageManager_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/UnionTypes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "nsIObserver.h"

class nsISmsService;
class nsIDOMMozSmsMessage;
class nsIDOMMozMmsMessage;
class nsIDOMMozSmsFilter;

namespace mozilla {
namespace dom {

class DOMRequest;
class DOMCursor;
struct MmsParameters;
struct MmsSendParameters;
struct SmsSendParameters;

class MobileMessageManager MOZ_FINAL : public DOMEventTargetHelper
                                     , public nsIObserver
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIOBSERVER

  NS_REALLY_FORWARD_NSIDOMEVENTTARGET(DOMEventTargetHelper)

  MobileMessageManager(nsPIDOMWindow* aWindow);

  void Init();
  void Shutdown();

  nsPIDOMWindow*
  GetParentObject() const { return GetOwner(); }

  // WrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  // WebIDL Interface
  already_AddRefed<DOMRequest>
  GetSegmentInfoForText(const nsAString& aText,
                        ErrorResult& aRv);

  already_AddRefed<DOMRequest>
  Send(const nsAString& aNumber,
       const nsAString& aText,
       const SmsSendParameters& aSendParams,
       ErrorResult& aRv);

  void
  Send(const Sequence<nsString>& aNumbers,
       const nsAString& aText,
       const SmsSendParameters& aSendParams,
       nsTArray<nsRefPtr<DOMRequest>>& aReturn,
       ErrorResult& aRv);

  already_AddRefed<DOMRequest>
  SendMMS(const MmsParameters& aParameters,
          const MmsSendParameters& aSendParams,
          ErrorResult& aRv);

  already_AddRefed<DOMRequest>
  GetMessage(int32_t aId,
             ErrorResult& aRv);

  already_AddRefed<DOMRequest>
  Delete(int32_t aId,
         ErrorResult& aRv);

  already_AddRefed<DOMRequest>
  Delete(nsIDOMMozSmsMessage* aMessage,
         ErrorResult& aRv);

  already_AddRefed<DOMRequest>
  Delete(nsIDOMMozMmsMessage* aMessage,
         ErrorResult& aRv);

  already_AddRefed<DOMRequest>
  Delete(const Sequence<OwningLongOrMozSmsMessageOrMozMmsMessage>& aParams,
         ErrorResult& aRv);

  already_AddRefed<DOMCursor>
  GetMessages(nsIDOMMozSmsFilter* aFilter,
              bool aReverse,
              ErrorResult& aRv);

  already_AddRefed<DOMRequest>
  MarkMessageRead(int32_t aId,
                  bool aRead,
                  bool aSendReadReport,
                  ErrorResult& aRv);

  already_AddRefed<DOMCursor>
  GetThreads(ErrorResult& aRv);

  already_AddRefed<DOMRequest>
  RetrieveMMS(int32_t aId,
              ErrorResult& aRv);

  already_AddRefed<DOMRequest>
  RetrieveMMS(nsIDOMMozMmsMessage* aMessage,
              ErrorResult& aRv);

  already_AddRefed<DOMRequest>
  GetSmscAddress(const Optional<uint32_t>& aServiceId,
                 ErrorResult& aRv);

  IMPL_EVENT_HANDLER(received)
  IMPL_EVENT_HANDLER(retrieving)
  IMPL_EVENT_HANDLER(sending)
  IMPL_EVENT_HANDLER(sent)
  IMPL_EVENT_HANDLER(failed)
  IMPL_EVENT_HANDLER(deliverysuccess)
  IMPL_EVENT_HANDLER(deliveryerror)
  IMPL_EVENT_HANDLER(readsuccess)
  IMPL_EVENT_HANDLER(readerror)

private:
  /**
   * Internal Send() method used to send one message.
   */
  already_AddRefed<DOMRequest>
  Send(nsISmsService* aSmsService,
       uint32_t aServiceId,
       const nsAString& aNumber,
       const nsAString& aText,
       ErrorResult& aRv);

  already_AddRefed<DOMRequest>
  Delete(int32_t* aIdArray,
         uint32_t aSize,
         ErrorResult& aRv);

  nsresult
  DispatchTrustedSmsEventToSelf(const char* aTopic,
                                const nsAString& aEventName,
                                nsISupports* aMsg);

  /**
   * Helper to get message ID from SMS/MMS Message object
   */
  nsresult
  GetMessageId(JSContext* aCx,
               const JS::Value& aMessage,
               int32_t* aId);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_mobilemessage_MobileMessageManager_h
