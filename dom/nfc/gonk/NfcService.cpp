/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NfcService.h"
#include <binder/Parcel.h>
#include "mozilla/ModuleUtils.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/dom/RootedDictionary.h"
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

nsLiteralString NfcTechString[] = {
  NS_LITERAL_STRING("NDEF"),
  NS_LITERAL_STRING("NDEF_WRITEABLE"),
  NS_LITERAL_STRING("NDEF_FORMATABLE"),
  NS_LITERAL_STRING("P2P"),
  NS_LITERAL_STRING("NFC_A"),
  NS_LITERAL_STRING("NFC_B"),
  NS_LITERAL_STRING("NFC_F"),
  NS_LITERAL_STRING("NFC_V"),
  NS_LITERAL_STRING("NFC_ISO_DEP"),
  NS_LITERAL_STRING("MIFARE_CLASSIC"),
  NS_LITERAL_STRING("MIFARE_ULTRALIGHT"),
  NS_LITERAL_STRING("BARCODE")
};

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

    COPY_FIELD(mType)
    COPY_OPT_FIELD(mRequestId, EmptyString())
    COPY_OPT_FIELD(mStatus, -1)
    COPY_OPT_FIELD(mSessionId, -1)
    COPY_OPT_FIELD(mMajorVersion, -1)
    COPY_OPT_FIELD(mMinorVersion, -1)
    COPY_OPT_FIELD(mPowerLevel, -1)

    if (mEvent.mTechList.Length() > 0) {
      int length = mEvent.mTechList.Length();
      event.mTechList.Construct();

      if (!event.mTechList.Value().SetCapacity(length)) {
        return NS_ERROR_FAILURE;
      }

      for (int i = 0; i < length; i++) {
        nsString& elem = *event.mTechList.Value().AppendElement();
        elem = NfcTechString[mEvent.mTechList[i]];
      }
    }

    if (mEvent.mRecords.Length() > 0) {
      int length = mEvent.mRecords.Length();
      event.mRecords.Construct();
      if (!event.mRecords.Value().SetCapacity(length)) {
        return NS_ERROR_FAILURE;
      }

      for (int i = 0; i < length; i++) {
        NDEFRecordStruct& recordStruct = mEvent.mRecords[i];
        NDEFRecord& record = *event.mRecords.Value().AppendElement();

        record.mTnf.Construct();
        record.mTnf.Value() = recordStruct.mTnf;

        if (recordStruct.mType.Length() > 0) {
          record.mType.Construct();
          record.mType.Value().Init(Uint8Array::Create(cx, recordStruct.mType.Length(), recordStruct.mType.Elements()));
        }

        if (recordStruct.mId.Length() > 0) {
          record.mId.Construct();
          record.mId.Value().Init(Uint8Array::Create(cx, recordStruct.mId.Length(), recordStruct.mId.Elements()));
        }

        if (recordStruct.mPayload.Length() > 0) {
          record.mPayload.Construct();
          record.mPayload.Value().Init(Uint8Array::Create(cx, recordStruct.mPayload.Length(), recordStruct.mPayload.Elements()));
        }
      }
    }

    COPY_OPT_FIELD(mIsReadOnly, -1)
    COPY_OPT_FIELD(mCanBeMadeReadOnly, -1)
    COPY_OPT_FIELD(mMaxSupportedLength, -1)

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

    size_t size = mData->mSize;
    size_t offset = 0;

    while (size > 0) {
      EventOptions event;
      const uint8_t* data = mData->mData.get();
      uint32_t parcelSize = ((data[offset + 0] & 0xff) << 24) |
                            ((data[offset + 1] & 0xff) << 16) |
                            ((data[offset + 2] & 0xff) <<  8) |
                             (data[offset + 3] & 0xff);
      MOZ_ASSERT(parcelSize <= (mData->mSize - offset));

      Parcel parcel;
      parcel.setData(&data[offset], parcelSize + sizeof(int));
      mHandler->Unmarshall(parcel, event);
      nsCOMPtr<nsIRunnable> runnable = new NfcEventDispatcher(event);
      NS_DispatchToMainThread(runnable);

      size -= parcel.dataSize();
      offset += parcel.dataSize();
    }

    return NS_OK;
  }

private:
  NfcMessageHandler* mHandler;
  nsAutoPtr<UnixSocketRawData> mData;
};

NfcService::NfcService()
  : mConsumer(new NfcConsumer(this))
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
NfcService::Start(nsINfcEventListener* aListener)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aListener);
  MOZ_ASSERT(!mThread);

  nsresult rv = NS_NewNamedThread("NfcThread", getter_AddRefs(mThread));
  if (NS_FAILED(rv)) {
    NS_WARNING("Can't create Nfc worker thread.");
    Shutdown();
    return NS_ERROR_FAILURE;
  }

  mListener = aListener;
  mHandler = new NfcMessageHandler();

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

  mConsumer->Shutdown();

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
