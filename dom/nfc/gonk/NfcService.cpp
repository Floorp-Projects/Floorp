/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NfcService.h"
#include <binder/Parcel.h>
#include <cutils/properties.h>
#include "mozilla/ModuleUtils.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/NfcOptionsBinding.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/dom/RootedDictionary.h"
#include "mozilla/unused.h"
#include "nsAutoPtr.h"
#include "nsString.h"
#include "nsXULAppAPI.h"
#include "NfcOptions.h"

#define NS_NFCSERVICE_CID \
  { 0x584c9a21, 0x4e17, 0x43b7, {0xb1, 0x6a, 0x87, 0xa0, 0x42, 0xef, 0xd4, 0x64} }
#define NS_NFCSERVICE_CONTRACTID "@mozilla.org/nfc/service;1"

using namespace android;
using namespace mozilla::dom;
using namespace mozilla::ipc;

namespace mozilla {

static NfcService* gNfcService;

NS_IMPL_ISUPPORTS(NfcService, nsINfcService)

void
assertIsNfcServiceThread()
{
  nsCOMPtr<nsIThread> thread = do_GetCurrentThread();
  MOZ_ASSERT(thread == gNfcService->GetThread());
}

// Runnable used to call Marshall on the NFC thread.
class NfcCommandRunnable : public nsRunnable
{
public:
  NfcCommandRunnable(NfcMessageHandler* aHandler, NfcConsumer* aConsumer, CommandOptions aOptions)
    : mHandler(aHandler), mConsumer(aConsumer), mOptions(aOptions)
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  NS_IMETHOD Run()
  {
    assertIsNfcServiceThread();

    Parcel parcel;
    parcel.writeInt32(0); // Parcel Size.
    mHandler->Marshall(parcel, mOptions);
    parcel.setDataPosition(0);
    uint32_t sizeBE = htonl(parcel.dataSize() - sizeof(int));
    parcel.writeInt32(sizeBE);
    mConsumer->PostToNfcDaemon(parcel.data(), parcel.dataSize());
    return NS_OK;
  }

private:
   NfcMessageHandler* mHandler;
   NfcConsumer* mConsumer;
   CommandOptions mOptions;
};

// Runnable used dispatch the NfcEventOptions on the main thread.
class NfcEventDispatcher : public nsRunnable
{
public:
  NfcEventDispatcher(EventOptions& aEvent)
    : mEvent(aEvent)
  {
    assertIsNfcServiceThread();
  }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());

    mozilla::AutoSafeJSContext cx;
    RootedDictionary<NfcEventOptions> event(cx);

    // the copy constructor is private.
#define COPY_FIELD(prop) event.prop = mEvent.prop;

#define COPY_OPT_FIELD(prop, defaultValue)           \
    if (mEvent.prop != defaultValue) {               \
      event.prop.Construct();                        \
      event.prop.Value() = mEvent.prop;              \
    }

    COPY_OPT_FIELD(mRspType, NfcResponseType::EndGuard_)
    COPY_OPT_FIELD(mNtfType, NfcNotificationType::EndGuard_)

    // Only one of rspType and ntfType should be used.
    MOZ_ASSERT(((mEvent.mRspType != NfcResponseType::EndGuard_) ||
                (mEvent.mNtfType != NfcNotificationType::EndGuard_)) &&
               ((mEvent.mRspType == NfcResponseType::EndGuard_) ||
                (mEvent.mNtfType == NfcNotificationType::EndGuard_)));

    COPY_OPT_FIELD(mRequestId, EmptyString())
    COPY_OPT_FIELD(mStatus, -1)
    COPY_OPT_FIELD(mSessionId, -1)
    COPY_OPT_FIELD(mMajorVersion, -1)
    COPY_OPT_FIELD(mMinorVersion, -1)

    if (mEvent.mRfState != -1) {
      event.mRfState.Construct();
      RFState rfState = static_cast<RFState>(mEvent.mRfState);
      MOZ_ASSERT(rfState < RFState::EndGuard_);
      event.mRfState.Value() = rfState;
    }

    if (mEvent.mErrorCode != -1) {
      event.mErrorMsg.Construct();
      event.mErrorMsg.Value() = static_cast<NfcErrorMessage>(mEvent.mErrorCode);
    }

    if (mEvent.mTechList.Length() > 0) {
      int length = mEvent.mTechList.Length();
      event.mTechList.Construct();

      if (!event.mTechList.Value().SetCapacity(length)) {
        return NS_ERROR_FAILURE;
      }

      for (int i = 0; i < length; i++) {
        NFCTechType tech = static_cast<NFCTechType>(mEvent.mTechList[i]);
        MOZ_ASSERT(tech < NFCTechType::EndGuard_);
        *event.mTechList.Value().AppendElement() = tech;
      }
    }

    if (mEvent.mTagId.Length() > 0) {
      event.mTagId.Construct();
      event.mTagId.Value().Init(Uint8Array::Create(cx, mEvent.mTagId.Length(), mEvent.mTagId.Elements()));
    }

    if (mEvent.mRecords.Length() > 0) {
      int length = mEvent.mRecords.Length();
      event.mRecords.Construct();
      if (!event.mRecords.Value().SetCapacity(length)) {
        return NS_ERROR_FAILURE;
      }

      for (int i = 0; i < length; i++) {
        NDEFRecordStruct& recordStruct = mEvent.mRecords[i];
        MozNDEFRecordOptions& record = *event.mRecords.Value().AppendElement();

        record.mTnf = recordStruct.mTnf;
        MOZ_ASSERT(record.mTnf < TNF::EndGuard_);

        if (recordStruct.mType.Length() > 0) {
          record.mType.Construct();
          record.mType.Value().SetValue().Init(Uint8Array::Create(cx, recordStruct.mType.Length(), recordStruct.mType.Elements()));
        }

        if (recordStruct.mId.Length() > 0) {
          record.mId.Construct();
          record.mId.Value().SetValue().Init(Uint8Array::Create(cx, recordStruct.mId.Length(), recordStruct.mId.Elements()));
        }

        if (recordStruct.mPayload.Length() > 0) {
          record.mPayload.Construct();
          record.mPayload.Value().SetValue().Init(Uint8Array::Create(cx, recordStruct.mPayload.Length(), recordStruct.mPayload.Elements()));
        }
      }
    }

    COPY_OPT_FIELD(mIsP2P, -1)

    if (mEvent.mTagType != -1) {
      event.mTagType.Construct();
      event.mTagType.Value() = static_cast<NFCTagType>(mEvent.mTagType);
    }

    COPY_OPT_FIELD(mMaxNDEFSize, -1)
    COPY_OPT_FIELD(mIsReadOnly, -1)
    COPY_OPT_FIELD(mIsFormatable, -1)

    // HCI Event Transaction parameters.
    if (mEvent.mOriginType != -1) {
      MOZ_ASSERT(static_cast<HCIEventOrigin>(mEvent.mOriginType) < HCIEventOrigin::EndGuard_);

      event.mOrigin.Construct();
      event.mOrigin.Value().AssignASCII(HCIEventOriginValues::strings[mEvent.mOriginType].value);
      event.mOrigin.Value().AppendInt(mEvent.mOriginIndex, 16 /* radix */);
    }

    if (mEvent.mAid.Length() > 0) {
      event.mAid.Construct();
      event.mAid.Value().Init(Uint8Array::Create(cx, mEvent.mAid.Length(), mEvent.mAid.Elements()));
    }

    if (mEvent.mPayload.Length() > 0) {
      event.mPayload.Construct();
      event.mPayload.Value().Init(Uint8Array::Create(cx, mEvent.mPayload.Length(), mEvent.mPayload.Elements()));
    }

    if (mEvent.mResponse.Length() > 0) {
      event.mResponse.Construct();
      event.mResponse.Value().Init(
        Uint8Array::Create(cx, mEvent.mResponse.Length(), mEvent.mResponse.Elements()));
    }

#undef COPY_FIELD
#undef COPY_OPT_FIELD

    gNfcService->DispatchNfcEvent(event);
    return NS_OK;
  }

private:
  EventOptions mEvent;
};

// Runnable used to call Unmarshall on the NFC thread.
class NfcEventRunnable : public nsRunnable
{
public:
  NfcEventRunnable(NfcMessageHandler* aHandler, UnixSocketRawData* aData)
    : mHandler(aHandler), mData(aData)
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  NS_IMETHOD Run()
  {
    assertIsNfcServiceThread();

    while (mData->GetSize()) {
      EventOptions event;
      const uint8_t* data = mData->GetData();
      uint32_t parcelSize = ((data[0] & 0xff) << 24) |
                            ((data[1] & 0xff) << 16) |
                            ((data[2] & 0xff) <<  8) |
                             (data[3] & 0xff);
      MOZ_ASSERT(parcelSize <= mData->GetSize());

      Parcel parcel;
      parcel.setData(mData->GetData(), parcelSize + sizeof(parcelSize));
      mHandler->Unmarshall(parcel, event);
      nsCOMPtr<nsIRunnable> runnable = new NfcEventDispatcher(event);
      NS_DispatchToMainThread(runnable);

      mData->Consume(parcelSize + sizeof(parcelSize));
    }

    return NS_OK;
  }

private:
  NfcMessageHandler* mHandler;
  nsAutoPtr<UnixSocketRawData> mData;
};

NfcService::NfcService()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!gNfcService);
}

NfcService::~NfcService()
{
  MOZ_ASSERT(!gNfcService);
}

already_AddRefed<NfcService>
NfcService::FactoryCreate()
{
  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    return nullptr;
  }

  MOZ_ASSERT(NS_IsMainThread());

  if (!gNfcService) {
    gNfcService = new NfcService();
    ClearOnShutdown(&gNfcService);
  }

  nsRefPtr<NfcService> service = gNfcService;
  return service.forget();
}

NS_IMETHODIMP
NfcService::Start(nsINfcGonkEventListener* aListener)
{
  static const char BASE_SOCKET_NAME[] = "nfcd";

  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aListener);
  MOZ_ASSERT(!mThread);
  MOZ_ASSERT(!mListenSocket);

  // If we could not cleanup properly before and an old
  // instance of the daemon is still running, we kill it
  // here.
  unused << NS_WARN_IF(property_set("ctl.stop", "nfcd") < 0);

  mListener = aListener;
  mHandler = new NfcMessageHandler();
  mConsumer = new NfcConsumer(this);

  mListenSocketName = BASE_SOCKET_NAME;

  mListenSocket = new NfcListenSocket(this);

  bool success = mListenSocket->Listen(new NfcConnector(), mConsumer);
  if (!success) {
    mConsumer = nullptr;
    return NS_ERROR_FAILURE;
  }

  nsresult rv = NS_NewNamedThread("NfcThread", getter_AddRefs(mThread));
  if (NS_FAILED(rv)) {
    NS_WARNING("Can't create Nfc worker thread.");
    mListenSocket->Close();
    mListenSocket = nullptr;
    mConsumer->Shutdown();
    mConsumer = nullptr;
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
NfcService::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mThread) {
    mThread->Shutdown();
    mThread = nullptr;
  }

  mListenSocket->Close();
  mListenSocket = nullptr;

  mConsumer->Shutdown();
  mConsumer = nullptr;

  return NS_OK;
}

NS_IMETHODIMP
NfcService::SendCommand(JS::HandleValue aOptions, JSContext* aCx)
{
  MOZ_ASSERT(NS_IsMainThread());

  NfcCommandOptions options;

  if (!options.Init(aCx, aOptions)) {
    NS_WARNING("Bad dictionary passed to NfcService::SendCommand");
    return NS_ERROR_FAILURE;
  }

  // Dispatch the command to the NFC thread.
  CommandOptions commandOptions(options);
  nsCOMPtr<nsIRunnable> runnable = new NfcCommandRunnable(mHandler, mConsumer, commandOptions);
  mThread->Dispatch(runnable, nsIEventTarget::DISPATCH_NORMAL);
  return NS_OK;
}

void
NfcService::DispatchNfcEvent(const mozilla::dom::NfcEventOptions& aOptions)
{
  MOZ_ASSERT(NS_IsMainThread());

  mozilla::AutoSafeJSContext cx;
  JS::RootedValue val(cx);

  if (!ToJSValue(cx, aOptions, &val)) {
    return;
  }

  mListener->OnEvent(val);
}

void
NfcService::ReceiveSocketData(nsAutoPtr<UnixSocketRawData>& aData)
{
  MOZ_ASSERT(mHandler);
  nsCOMPtr<nsIRunnable> runnable = new NfcEventRunnable(mHandler, aData.forget());
  mThread->Dispatch(runnable, nsIEventTarget::DISPATCH_NORMAL);
}

void
NfcService::OnConnectSuccess(enum SocketType aSocketType)
{
  MOZ_ASSERT(NS_IsMainThread());

  switch (aSocketType) {
    case LISTEN_SOCKET: {
        nsCString value("nfcd:-a ");
        value.Append(mListenSocketName);
        if (NS_WARN_IF(property_set("ctl.start", value.get()) < 0)) {
          OnConnectError(STREAM_SOCKET);
        }
      }
      break;
    case STREAM_SOCKET:
      /* nothing to do */
      break;
  }
}

void
NfcService::OnConnectError(enum SocketType aSocketType)
{
  MOZ_ASSERT(NS_IsMainThread());

  Shutdown();
}

void
NfcService::OnDisconnect(enum SocketType aSocketType)
{
  MOZ_ASSERT(NS_IsMainThread());
}

NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(NfcService,
                                         NfcService::FactoryCreate)

NS_DEFINE_NAMED_CID(NS_NFCSERVICE_CID);

static const mozilla::Module::CIDEntry kNfcServiceCIDs[] = {
  { &kNS_NFCSERVICE_CID, false, nullptr, NfcServiceConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kNfcServiceContracts[] = {
  { NS_NFCSERVICE_CONTRACTID, &kNS_NFCSERVICE_CID },
  { nullptr }
};

static const mozilla::Module kNfcServiceModule = {
  mozilla::Module::kVersion,
  kNfcServiceCIDs,
  kNfcServiceContracts,
  nullptr
};

} // namespace mozilla

NSMODULE_DEFN(NfcServiceModule) = &mozilla::kNfcServiceModule;
