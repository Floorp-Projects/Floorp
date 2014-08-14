/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// XXX: This must be done prior to including cert.h (directly or indirectly).
// CERT_AddTempCertToPerm is exposed as __CERT_AddTempCertToPerm.
#define CERT_AddTempCertToPerm __CERT_AddTempCertToPerm

#include "WifiCertService.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/ModuleUtils.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/ToJSValue.h"
#include "cert.h"
#include "certdb.h"
#include "CryptoTask.h"
#include "nsCxPusher.h"
#include "nsIDOMFile.h"
#include "nsIWifiService.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "nsXULAppAPI.h"
#include "ScopedNSSTypes.h"

#define NS_WIFICERTSERVICE_CID \
  { 0x83585afd, 0x0e11, 0x43aa, {0x83, 0x46, 0xf3, 0x4d, 0x97, 0x5e, 0x46, 0x77} }

using namespace mozilla;
using namespace mozilla::dom;

namespace mozilla {

// The singleton Wifi Cert service, to be used on the main thread.
StaticRefPtr<WifiCertService> gWifiCertService;

class ImportCertTask MOZ_FINAL: public CryptoTask
{
public:
  ImportCertTask(int32_t aId, nsIDOMBlob* aCertBlob,
                 const nsAString& aCertPassword,
                 const nsAString& aCertNickname)
    : mBlob(aCertBlob)
    , mPassword(aCertPassword)
  {
    MOZ_ASSERT(NS_IsMainThread());

    mResult.mId = aId;
    mResult.mStatus = 0;
    mResult.mUsageFlag = 0;
    mResult.mNickname = aCertNickname;
  }

private:
  virtual void ReleaseNSSResources() {}

  virtual nsresult CalculateResult() MOZ_OVERRIDE
  {
    MOZ_ASSERT(!NS_IsMainThread());

    // read data from blob.
    nsCString blobBuf;
    nsresult rv = ReadBlob(blobBuf);
    if (NS_FAILED(rv)) {
      return rv;
    }

    char* buf;
    uint32_t size = blobBuf.GetMutableData(&buf);
    if (size == 0) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    // Only support DER format now.
    return ImportDERBlob(buf, size, mResult.mNickname,
                         &mResult.mUsageFlag);
  }

  virtual void CallCallback(nsresult rv)
  {
    if (NS_FAILED(rv)) {
      mResult.mStatus = -1;
    }
    gWifiCertService->DispatchResult(mResult);
  }

  nsresult ImportDERBlob(char* buf, uint32_t size,
                         const nsAString& aNickname,
                         /*out*/ uint16_t* aUsageFlag)
  {
    NS_ENSURE_ARG_POINTER(aUsageFlag);

    // Create certificate object.
    ScopedCERTCertificate cert(CERT_DecodeCertFromPackage(buf, size));
    if (!cert) {
      return MapSECStatus(SECFailure);
    }

    // Import certificate with nickname.
    return ImportCert(cert, aNickname, aUsageFlag);
  }

  nsresult ReadBlob(/*out*/ nsCString& aBuf)
  {
    NS_ENSURE_ARG_POINTER(mBlob);

    static const uint64_t MAX_FILE_SIZE = 16384;
    uint64_t size;
    nsresult rv = mBlob->GetSize(&size);
    if (NS_FAILED(rv)) {
      return rv;
    }
    if (size > MAX_FILE_SIZE) {
      return NS_ERROR_FILE_TOO_BIG;
    }

    nsCOMPtr<nsIInputStream> inputStream;
    rv = mBlob->GetInternalStream(getter_AddRefs(inputStream));
    if (NS_FAILED(rv)) {
      return rv;
    }

    rv = NS_ReadInputStreamToString(inputStream, aBuf, (uint32_t)size);
    if (NS_FAILED(rv)) {
      return rv;
    }

    return NS_OK;
  }

  nsresult ImportCert(CERTCertificate* aCert, const nsAString& aNickname,
                      /*out*/ uint16_t* aUsageFlag)
  {
    NS_ENSURE_ARG_POINTER(aUsageFlag);

    nsCString userNickname, fullNickname;

    CopyUTF16toUTF8(aNickname, userNickname);
    // Determine certificate nickname by adding prefix according to its type.
    if (aCert->isRoot && (aCert->nsCertType & NS_CERT_TYPE_SSL_CA)) {
      // Accept self-signed SSL CA as server certificate.
      fullNickname.AssignLiteral("WIFI_SERVERCERT_");
      fullNickname += userNickname;
      *aUsageFlag |= nsIWifiCertService::WIFI_CERT_USAGE_FLAG_SERVER;
    } else {
      return NS_ERROR_ABORT;
    }

    char* nickname;
    uint32_t length;
    length = fullNickname.GetMutableData(&nickname);
    if (length == 0) {
      return NS_ERROR_UNEXPECTED;
    }

    // Import certificate, duplicated nickname will cause error.
    SECStatus srv = CERT_AddTempCertToPerm(aCert, nickname, NULL);
    if (srv != SECSuccess) {
      return MapSECStatus(srv);
    }

    return NS_OK;
  }

  nsCOMPtr<nsIDOMBlob> mBlob;
  nsString mPassword;
  WifiCertServiceResultOptions mResult;
};

class DeleteCertTask MOZ_FINAL: public CryptoTask
{
public:
  DeleteCertTask(int32_t aId, const nsAString& aCertNickname)
  {
    MOZ_ASSERT(NS_IsMainThread());

    mResult.mId = aId;
    mResult.mStatus = 0;
    mResult.mUsageFlag = 0;
    mResult.mNickname = aCertNickname;
  }

private:
  virtual void ReleaseNSSResources() {}

  virtual nsresult CalculateResult() MOZ_OVERRIDE
  {
    MOZ_ASSERT(!NS_IsMainThread());

    nsCString userNickname;
    CopyUTF16toUTF8(mResult.mNickname, userNickname);

    // Delete server certificate.
    nsCString serverCertName("WIFI_SERVERCERT_", 16);
    serverCertName += userNickname;

    ScopedCERTCertificate cert(
      CERT_FindCertByNickname(CERT_GetDefaultCertDB(), serverCertName.get())
    );
    if (!cert) {
      return MapSECStatus(SECFailure);
    }

    SECStatus srv = SEC_DeletePermCertificate(cert);
    if (srv != SECSuccess) {
      return MapSECStatus(srv);
    }

    return NS_OK;
  }

  virtual void CallCallback(nsresult rv)
  {
    if (NS_FAILED(rv)) {
      mResult.mStatus = -1;
    }
    gWifiCertService->DispatchResult(mResult);
  }

  WifiCertServiceResultOptions mResult;
};

NS_IMPL_ISUPPORTS(WifiCertService, nsIWifiCertService)

NS_IMETHODIMP
WifiCertService::Start(nsIWifiEventListener* aListener)
{
  MOZ_ASSERT(aListener);

  nsresult rv = NS_NewThread(getter_AddRefs(mRequestThread));
  if (NS_FAILED(rv)) {
    NS_WARNING("Certn't create wifi control thread");
    Shutdown();
    return NS_ERROR_FAILURE;
  }

  mListener = aListener;

  return NS_OK;
}

NS_IMETHODIMP
WifiCertService::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mRequestThread) {
    mRequestThread->Shutdown();
    mRequestThread = nullptr;
  }

  mListener = nullptr;

  return NS_OK;
}

void
WifiCertService::DispatchResult(const WifiCertServiceResultOptions& aOptions)
{
  MOZ_ASSERT(NS_IsMainThread());

  mozilla::AutoSafeJSContext cx;
  JS::RootedValue val(cx);
  nsCString dummyInterface;

  if (!ToJSValue(cx, aOptions, &val)) {
    return;
  }

  // Certll the listener with a JS value.
  mListener->OnCommand(val, dummyInterface);
}

WifiCertService::WifiCertService()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!gWifiCertService);
}

WifiCertService::~WifiCertService()
{
  MOZ_ASSERT(!gWifiCertService);
}

already_AddRefed<WifiCertService>
WifiCertService::FactoryCreate()
{
  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    return nullptr;
  }

  MOZ_ASSERT(NS_IsMainThread());

  if (!gWifiCertService) {
    gWifiCertService = new WifiCertService();
    ClearOnShutdown(&gWifiCertService);
  }

  nsRefPtr<WifiCertService> service = gWifiCertService.get();
  return service.forget();
}

NS_IMETHODIMP
WifiCertService::ImportCert(int32_t aId, nsIDOMBlob* aCertBlob,
                            const nsAString& aCertPassword,
                            const nsAString& aCertNickname)
{
  RefPtr<CryptoTask> task = new ImportCertTask(aId, aCertBlob, aCertPassword,
                                               aCertNickname);
  return task->Dispatch("WifiImportCert");
}

NS_IMETHODIMP
WifiCertService::DeleteCert(int32_t aId, const nsAString& aCertNickname)
{
  RefPtr<CryptoTask> task = new DeleteCertTask(aId, aCertNickname);
  return task->Dispatch("WifiDeleteCert");
}

NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(WifiCertService,
                                         WifiCertService::FactoryCreate)

NS_DEFINE_NAMED_CID(NS_WIFICERTSERVICE_CID);

static const mozilla::Module::CIDEntry kWifiCertServiceCIDs[] = {
  { &kNS_WIFICERTSERVICE_CID, false, nullptr, WifiCertServiceConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kWifiCertServiceContracts[] = {
  { "@mozilla.org/wifi/certservice;1", &kNS_WIFICERTSERVICE_CID },
  { nullptr }
};

static const mozilla::Module kWifiCertServiceModule = {
  mozilla::Module::kVersion,
  kWifiCertServiceCIDs,
  kWifiCertServiceContracts,
  nullptr
};

} // namespace mozilla

NSMODULE_DEFN(WifiCertServiceModule) = &kWifiCertServiceModule;
