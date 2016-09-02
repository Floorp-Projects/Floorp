/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ContentChild.h"
#include "SmsIPCService.h"
#include "nsXULAppAPI.h"
#include "mozilla/dom/mobilemessage/SmsChild.h"
#include "nsJSUtils.h"
#include "mozilla/dom/MozMobileMessageManagerBinding.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/Preferences.h"
#include "nsString.h"
#include "mozilla/dom/ipc/BlobChild.h"
#include "mozilla/Unused.h"

using namespace mozilla::dom;
using namespace mozilla::dom::mobilemessage;

namespace {

#define kPrefMmsDefaultServiceId "dom.mms.defaultServiceId"
#define kPrefSmsDefaultServiceId "dom.sms.defaultServiceId"

// TODO: Bug 767082 - WebSMS: sSmsChild leaks at shutdown
PSmsChild* gSmsChild;

// SmsIPCService is owned by nsLayoutModule.
SmsIPCService* sSingleton = nullptr;

PSmsChild*
GetSmsChild()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!gSmsChild) {
    gSmsChild = ContentChild::GetSingleton()->SendPSmsConstructor();

    NS_WARNING_ASSERTION(gSmsChild,
                         "Calling methods on SmsIPCService during shutdown!");
  }

  return gSmsChild;
}

nsresult
SendRequest(const IPCSmsRequest& aRequest,
            nsIMobileMessageCallback* aRequestReply)
{
  PSmsChild* smsChild = GetSmsChild();
  NS_ENSURE_TRUE(smsChild, NS_ERROR_FAILURE);

  SmsRequestChild* actor = new SmsRequestChild(aRequestReply);
  smsChild->SendPSmsRequestConstructor(actor, aRequest);

  return NS_OK;
}

nsresult
SendCursorRequest(const IPCMobileMessageCursor& aRequest,
                  nsIMobileMessageCursorCallback* aRequestReply,
                  nsICursorContinueCallback** aResult)
{
  PSmsChild* smsChild = GetSmsChild();
  NS_ENSURE_TRUE(smsChild, NS_ERROR_FAILURE);

  RefPtr<MobileMessageCursorChild> actor =
    new MobileMessageCursorChild(aRequestReply);

  // Add an extra ref for IPDL. Will be released in
  // SmsChild::DeallocPMobileMessageCursor().
  RefPtr<MobileMessageCursorChild> actorCopy(actor);
  mozilla::Unused << actorCopy.forget().take();

  smsChild->SendPMobileMessageCursorConstructor(actor, aRequest);

  actor.forget(aResult);
  return NS_OK;
}

uint32_t
getDefaultServiceId(const char* aPrefKey)
{
  static const char* kPrefRilNumRadioInterfaces = "ril.numRadioInterfaces";
  int32_t id = mozilla::Preferences::GetInt(aPrefKey, 0);
  int32_t numRil = mozilla::Preferences::GetInt(kPrefRilNumRadioInterfaces, 1);

  if (id >= numRil || id < 0) {
    id = 0;
  }

  return id;
}

} // namespace

NS_IMPL_ISUPPORTS(SmsIPCService,
                  nsISmsService,
                  nsIMmsService,
                  nsIMobileMessageDatabaseService,
                  nsIObserver)

/* static */ already_AddRefed<SmsIPCService>
SmsIPCService::GetSingleton()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!sSingleton) {
    sSingleton = new SmsIPCService();
  }

  RefPtr<SmsIPCService> service = sSingleton;
  return service.forget();
}

SmsIPCService::SmsIPCService()
{
  static const char* kObservedPrefs[] = {
    kPrefMmsDefaultServiceId,
    kPrefSmsDefaultServiceId,
    nullptr
  };
  Preferences::AddStrongObservers(this, kObservedPrefs);
  mMmsDefaultServiceId = getDefaultServiceId(kPrefMmsDefaultServiceId);
  mSmsDefaultServiceId = getDefaultServiceId(kPrefSmsDefaultServiceId);
}

SmsIPCService::~SmsIPCService()
{
  sSingleton = nullptr;
}

/*
 * Implementation of nsIObserver.
 */

NS_IMETHODIMP
SmsIPCService::Observe(nsISupports* aSubject,
                       const char* aTopic,
                       const char16_t* aData)
{
  if (!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    nsDependentString data(aData);
    if (data.EqualsLiteral(kPrefMmsDefaultServiceId)) {
      mMmsDefaultServiceId = getDefaultServiceId(kPrefMmsDefaultServiceId);
    } else if (data.EqualsLiteral(kPrefSmsDefaultServiceId)) {
      mSmsDefaultServiceId = getDefaultServiceId(kPrefSmsDefaultServiceId);
    }
    return NS_OK;
  }

  MOZ_ASSERT(false, "SmsIPCService got unexpected topic!");
  return NS_ERROR_UNEXPECTED;
}

/*
 * Implementation of nsISmsService.
 */

NS_IMETHODIMP
SmsIPCService::GetSmsDefaultServiceId(uint32_t* aServiceId)
{
  *aServiceId = mSmsDefaultServiceId;
  return NS_OK;
}

NS_IMETHODIMP
SmsIPCService::GetSegmentInfoForText(const nsAString& aText,
                                     nsIMobileMessageCallback* aRequest)
{
  return SendRequest(GetSegmentInfoForTextRequest(nsString(aText)),
                                                  aRequest);
}

NS_IMETHODIMP
SmsIPCService::GetSmscAddress(uint32_t aServiceId,
                              nsIMobileMessageCallback* aRequest)
{
  return SendRequest(GetSmscAddressRequest(aServiceId), aRequest);
}


NS_IMETHODIMP
SmsIPCService::SetSmscAddress(uint32_t aServiceId,
                              const nsAString& aNumber,
                              uint32_t aTypeOfNumber,
                              uint32_t aNumberPlanIdentification,
                              nsIMobileMessageCallback* aRequest)
{
  return SendRequest(SetSmscAddressRequest(aServiceId,
                                           nsString(aNumber),
                                           aTypeOfNumber,
                                           aNumberPlanIdentification),
                     aRequest);
}

NS_IMETHODIMP
SmsIPCService::Send(uint32_t aServiceId,
                    const nsAString& aNumber,
                    const nsAString& aMessage,
                    bool aSilent,
                    nsIMobileMessageCallback* aRequest)
{
  return SendRequest(SendMessageRequest(SendSmsMessageRequest(aServiceId,
                                                              nsString(aNumber),
                                                              nsString(aMessage),
                                                              aSilent)),
                     aRequest);
}

NS_IMETHODIMP
SmsIPCService::AddSilentNumber(const nsAString& aNumber)
{
  PSmsChild* smsChild = GetSmsChild();
  NS_ENSURE_TRUE(smsChild, NS_ERROR_FAILURE);

  smsChild->SendAddSilentNumber(nsString(aNumber));
  return NS_OK;
}

NS_IMETHODIMP
SmsIPCService::RemoveSilentNumber(const nsAString& aNumber)
{
  PSmsChild* smsChild = GetSmsChild();
  NS_ENSURE_TRUE(smsChild, NS_ERROR_FAILURE);

  smsChild->SendRemoveSilentNumber(nsString(aNumber));
  return NS_OK;
}

/*
 * Implementation of nsIMobileMessageDatabaseService.
 */
NS_IMETHODIMP
SmsIPCService::GetMessageMoz(int32_t aMessageId,
                             nsIMobileMessageCallback* aRequest)
{
  return SendRequest(GetMessageRequest(aMessageId), aRequest);
}

NS_IMETHODIMP
SmsIPCService::DeleteMessage(int32_t *aMessageIds, uint32_t aSize,
                             nsIMobileMessageCallback* aRequest)
{
  DeleteMessageRequest data;
  data.messageIds().AppendElements(aMessageIds, aSize);
  return SendRequest(data, aRequest);
}

NS_IMETHODIMP
SmsIPCService::CreateMessageCursor(bool aHasStartDate,
                                   uint64_t aStartDate,
                                   bool aHasEndDate,
                                   uint64_t aEndDate,
                                   const char16_t** aNumbers,
                                   uint32_t aNumbersCount,
                                   const nsAString& aDelivery,
                                   bool aHasRead,
                                   bool aRead,
                                   bool aHasThreadId,
                                   uint64_t aThreadId,
                                   bool aReverse,
                                   nsIMobileMessageCursorCallback* aCursorCallback,
                                   nsICursorContinueCallback** aResult)
{
  SmsFilterData data;

  data.hasStartDate() = aHasStartDate;
  data.startDate() = aStartDate;
  data.hasEndDate() = aHasEndDate;
  data.endDate() = aEndDate;

  if (aNumbersCount && aNumbers) {
    nsTArray<nsString>& numbers = data.numbers();
    uint32_t index;

    for (index = 0; index < aNumbersCount; index++) {
      numbers.AppendElement(aNumbers[index]);
    }
  }

  data.delivery() = aDelivery;
  data.hasRead() = aHasRead;
  data.read() = aRead;
  data.hasThreadId() = aHasThreadId;
  data.threadId() = aThreadId;

  return SendCursorRequest(CreateMessageCursorRequest(data, aReverse),
                           aCursorCallback, aResult);
}

NS_IMETHODIMP
SmsIPCService::MarkMessageRead(int32_t aMessageId,
                               bool aValue,
                               bool aSendReadReport,
                               nsIMobileMessageCallback* aRequest)
{
  return SendRequest(MarkMessageReadRequest(aMessageId, aValue, aSendReadReport), aRequest);
}

NS_IMETHODIMP
SmsIPCService::CreateThreadCursor(nsIMobileMessageCursorCallback* aCursorCallback,
                                  nsICursorContinueCallback** aResult)
{
  return SendCursorRequest(CreateThreadCursorRequest(), aCursorCallback,
                           aResult);
}

bool
GetSendMmsMessageRequestFromParams(uint32_t aServiceId,
                                   const JS::Value& aParam,
                                   SendMmsMessageRequest& request) {
  if (aParam.isUndefined() || aParam.isNull() || !aParam.isObject()) {
    return false;
  }

  mozilla::AutoJSContext cx;
  JS::Rooted<JS::Value> param(cx, aParam);
  RootedDictionary<MmsParameters> params(cx);
  if (!params.Init(cx, param)) {
    return false;
  }

  // SendMobileMessageRequest.receivers
  if (!params.mReceivers.WasPassed()) {
    return false;
  }
  request.receivers().AppendElements(params.mReceivers.Value());

  // SendMobileMessageRequest.attachments
  mozilla::dom::ContentChild* cc = mozilla::dom::ContentChild::GetSingleton();

  if (!params.mAttachments.WasPassed()) {
    return false;
  }

  for (uint32_t i = 0; i < params.mAttachments.Value().Length(); i++) {
    mozilla::dom::MmsAttachment& attachment = params.mAttachments.Value()[i];
    MmsAttachmentData mmsAttachment;
    mmsAttachment.id().Assign(attachment.mId);
    mmsAttachment.location().Assign(attachment.mLocation);
    mmsAttachment.contentChild() = cc->GetOrCreateActorForBlob(attachment.mContent);
    if (!mmsAttachment.contentChild()) {
      return false;
    }
    request.attachments().AppendElement(mmsAttachment);
  }

  request.smil() = params.mSmil;
  request.subject() = params.mSubject;

  // Set service ID.
  request.serviceId() = aServiceId;

  return true;
}

/*
 * Implementation of nsIMmsService.
 */

NS_IMETHODIMP
SmsIPCService::GetMmsDefaultServiceId(uint32_t* aServiceId)
{
  *aServiceId = mMmsDefaultServiceId;
  return NS_OK;
}

NS_IMETHODIMP
SmsIPCService::Send(uint32_t aServiceId,
                    JS::Handle<JS::Value> aParameters,
                    nsIMobileMessageCallback *aRequest)
{
  SendMmsMessageRequest req;
  if (!GetSendMmsMessageRequestFromParams(aServiceId, aParameters, req)) {
    return NS_ERROR_INVALID_ARG;
  }
  return SendRequest(SendMessageRequest(req), aRequest);
}

NS_IMETHODIMP
SmsIPCService::Retrieve(int32_t aId, nsIMobileMessageCallback *aRequest)
{
  return SendRequest(RetrieveMessageRequest(aId), aRequest);
}

NS_IMETHODIMP
SmsIPCService::SendReadReport(const nsAString & messageID,
                              const nsAString & toAddress,
                              const nsAString & iccId)
{
  NS_ERROR("We should not be here!");
  return NS_OK;
}
