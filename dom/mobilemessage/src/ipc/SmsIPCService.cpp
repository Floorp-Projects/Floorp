/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ContentChild.h"
#include "SmsIPCService.h"
#include "nsXULAppAPI.h"
#include "mozilla/dom/mobilemessage/SmsChild.h"
#include "SmsMessage.h"
#include "SmsFilter.h"
#include "SmsSegmentInfo.h"
#include "nsJSUtils.h"
#include "nsCxPusher.h"
#include "mozilla/dom/MobileMessageManagerBinding.h"
#include "mozilla/dom/MozMmsMessageBinding.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/Preferences.h"
#include "nsString.h"

using namespace mozilla::dom;
using namespace mozilla::dom::mobilemessage;

namespace {

const char* kPrefRilNumRadioInterfaces = "ril.numRadioInterfaces";
#define kPrefMmsDefaultServiceId "dom.mms.defaultServiceId"
#define kPrefSmsDefaultServiceId "dom.sms.defaultServiceId"
const char* kObservedPrefs[] = {
  kPrefMmsDefaultServiceId,
  kPrefSmsDefaultServiceId,
  nullptr
};

// TODO: Bug 767082 - WebSMS: sSmsChild leaks at shutdown
PSmsChild* gSmsChild;

PSmsChild*
GetSmsChild()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!gSmsChild) {
    gSmsChild = ContentChild::GetSingleton()->SendPSmsConstructor();

    NS_WARN_IF_FALSE(gSmsChild,
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

  nsRefPtr<MobileMessageCursorChild> actor =
    new MobileMessageCursorChild(aRequestReply);

  // Add an extra ref for IPDL. Will be released in
  // SmsChild::DeallocPMobileMessageCursor().
  actor->AddRef();

  smsChild->SendPMobileMessageCursorConstructor(actor, aRequest);

  actor.forget(aResult);
  return NS_OK;
}

uint32_t
getDefaultServiceId(const char* aPrefKey)
{
  int32_t id = mozilla::Preferences::GetInt(aPrefKey, 0);
  int32_t numRil = mozilla::Preferences::GetInt(kPrefRilNumRadioInterfaces, 1);

  if (id >= numRil || id < 0) {
    id = 0;
  }

  return id;
}

} // anonymous namespace

NS_IMPL_ISUPPORTS4(SmsIPCService,
                   nsISmsService,
                   nsIMmsService,
                   nsIMobileMessageDatabaseService,
                   nsIObserver)

SmsIPCService::SmsIPCService()
{
  Preferences::AddStrongObservers(this, kObservedPrefs);
  mMmsDefaultServiceId = getDefaultServiceId(kPrefMmsDefaultServiceId);
  mSmsDefaultServiceId = getDefaultServiceId(kPrefSmsDefaultServiceId);
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
SmsIPCService::IsSilentNumber(const nsAString& aNumber,
                              bool*            aIsSilent)
{
  NS_ERROR("We should not be here!");
  return NS_ERROR_FAILURE;
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
SmsIPCService::CreateMessageCursor(nsIDOMMozSmsFilter* aFilter,
                                   bool aReverse,
                                   nsIMobileMessageCursorCallback* aCursorCallback,
                                   nsICursorContinueCallback** aResult)
{
  const SmsFilterData& data =
    SmsFilterData(static_cast<SmsFilter*>(aFilter)->GetData());

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
