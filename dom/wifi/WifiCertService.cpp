/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// XXX: This must be done prior to including cert.h (directly or indirectly).
// CERT_AddTempCertToPerm is exposed as __CERT_AddTempCertToPerm.
#define CERT_AddTempCertToPerm __CERT_AddTempCertToPerm

#include "WifiCertService.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Endian.h"
#include "mozilla/ModuleUtils.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/ToJSValue.h"
#include "cert.h"
#include "certdb.h"
#include "CryptoTask.h"
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

class ImportCertTask final: public CryptoTask
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

  virtual nsresult CalculateResult() override
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

    // Try import as DER format first.
    rv = ImportDERBlob(buf, size);
    if (NS_SUCCEEDED(rv)) {
      return rv;
    }

    // Try import as PKCS#12 format.
    return ImportPKCS12Blob(buf, size, mPassword);
  }

  virtual void CallCallback(nsresult rv)
  {
    if (NS_FAILED(rv)) {
      mResult.mStatus = -1;
    }
    gWifiCertService->DispatchResult(mResult);
  }

  nsresult ImportDERBlob(char* buf, uint32_t size)
  {
    // Create certificate object.
    ScopedCERTCertificate cert(CERT_DecodeCertFromPackage(buf, size));
    if (!cert) {
      return MapSECStatus(SECFailure);
    }

    // Import certificate.
    return ImportCert(cert);
  }

  static SECItem*
  HandleNicknameCollision(SECItem* aOldNickname, PRBool* aCancel, void* aWincx)
  {
    const char* dummyName = "Imported User Cert";
    const size_t dummyNameLen = strlen(dummyName);
    SECItem* newNick = ::SECITEM_AllocItem(nullptr, nullptr, dummyNameLen + 1);
    if (!newNick) {
      return nullptr;
    }

    newNick->type = siAsciiString;
    // Dummy name, will be renamed later.
    memcpy(newNick->data, dummyName, dummyNameLen + 1);
    newNick->len = dummyNameLen;

    return newNick;
  }

  static SECStatus
  HandleNicknameUpdate(const CERTCertificate *aCert,
                       const SECItem *default_nickname,
                       SECItem **new_nickname,
                       void *arg)
  {
    WifiCertServiceResultOptions *result = (WifiCertServiceResultOptions *)arg;

    nsCString userNickname;
    CopyUTF16toUTF8(result->mNickname, userNickname);

    nsCString fullNickname;
    if (aCert->isRoot && (aCert->nsCertType & NS_CERT_TYPE_SSL_CA)) {
      // Accept self-signed SSL CA as server certificate.
      fullNickname.AssignLiteral("WIFI_SERVERCERT_");
      fullNickname += userNickname;
      result->mUsageFlag |= nsIWifiCertService::WIFI_CERT_USAGE_FLAG_SERVER;
    } else if (aCert->nsCertType & NS_CERT_TYPE_SSL_CLIENT) {
      // User Certificate
      fullNickname.AssignLiteral("WIFI_USERCERT_");
      fullNickname += userNickname;
      result->mUsageFlag |= nsIWifiCertService::WIFI_CERT_USAGE_FLAG_USER;
    }
    char* nickname;
    uint32_t length = fullNickname.GetMutableData(&nickname);

    SECItem* newNick = ::SECITEM_AllocItem(nullptr, nullptr, length + 1);
    if (!newNick) {
      return SECFailure;
    }

    newNick->type = siAsciiString;
    memcpy(newNick->data, nickname, length + 1);
    newNick->len = length;

    *new_nickname = newNick;
    return SECSuccess;
  }

  nsresult ImportPKCS12Blob(char* buf, uint32_t size, const nsAString& aPassword)
  {
    nsString password(aPassword);

    // password is null-terminated wide-char string.
    // passwordItem is required to be big-endian form of password, stored in char
    // array, including the null-termination.
    uint32_t length = password.Length() + 1;
    ScopedSECItem passwordItem(
      ::SECITEM_AllocItem(nullptr, nullptr, length * sizeof(nsString::char_type)));

    if (!passwordItem) {
      return NS_ERROR_FAILURE;
    }

    mozilla::NativeEndian::copyAndSwapToBigEndian(passwordItem->data,
                                                  password.BeginReading(),
                                                  length);
    // Create a decoder.
    ScopedSEC_PKCS12DecoderContext p12dcx(SEC_PKCS12DecoderStart(
                                            passwordItem, nullptr, nullptr,
                                            nullptr, nullptr, nullptr, nullptr,
                                            nullptr));

    if (!p12dcx) {
      return NS_ERROR_FAILURE;
    }

    // Assign data to decorder.
    SECStatus srv = SEC_PKCS12DecoderUpdate(p12dcx,
                                            reinterpret_cast<unsigned char*>(buf),
                                            size);
    if (srv != SECSuccess) {
      return MapSECStatus(srv);
    }

    // Verify certificates.
    srv = SEC_PKCS12DecoderVerify(p12dcx);
    if (srv != SECSuccess) {
      return MapSECStatus(srv);
    }

    // Set certificate nickname and usage flag.
    srv = SEC_PKCS12DecoderRenameCertNicknames(p12dcx, HandleNicknameUpdate,
                                               &mResult);

    // Validate certificates.
    srv = SEC_PKCS12DecoderValidateBags(p12dcx, HandleNicknameCollision);
    if (srv != SECSuccess) {
      return MapSECStatus(srv);
    }

    // Initialize slot.
    ScopedPK11SlotInfo slot(PK11_GetInternalKeySlot());
    if (!slot) {
      return NS_ERROR_FAILURE;
    }
    if (PK11_NeedLogin(slot) && PK11_NeedUserInit(slot)) {
      srv = PK11_InitPin(slot, "", "");
      if (srv != SECSuccess) {
        return MapSECStatus(srv);
      }
    }

    // Import cert and key.
    srv = SEC_PKCS12DecoderImportBags(p12dcx);
    if (srv != SECSuccess) {
      return MapSECStatus(srv);
    }

    // User certificate must be imported from PKCS#12.
    return (mResult.mUsageFlag & nsIWifiCertService::WIFI_CERT_USAGE_FLAG_USER)
            ? NS_OK : NS_ERROR_FAILURE;
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

  nsresult ImportCert(CERTCertificate* aCert)
  {
    nsCString userNickname, fullNickname;

    CopyUTF16toUTF8(mResult.mNickname, userNickname);
    // Determine certificate nickname by adding prefix according to its type.
    if (aCert->isRoot && (aCert->nsCertType & NS_CERT_TYPE_SSL_CA)) {
      // Accept self-signed SSL CA as server certificate.
      fullNickname.AssignLiteral("WIFI_SERVERCERT_");
      fullNickname += userNickname;
      mResult.mUsageFlag |= nsIWifiCertService::WIFI_CERT_USAGE_FLAG_SERVER;
    } else if (aCert->nsCertType & NS_CERT_TYPE_SSL_CLIENT) {
      // User Certificate
      fullNickname.AssignLiteral("WIFI_USERCERT_");
      fullNickname += userNickname;
      mResult.mUsageFlag |= nsIWifiCertService::WIFI_CERT_USAGE_FLAG_USER;
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
    SECStatus srv = CERT_AddTempCertToPerm(aCert, nickname, nullptr);
    if (srv != SECSuccess) {
      return MapSECStatus(srv);
    }

    return NS_OK;
  }

  nsCOMPtr<nsIDOMBlob> mBlob;
  nsString mPassword;
  WifiCertServiceResultOptions mResult;
};

class DeleteCertTask final: public CryptoTask
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

  virtual nsresult CalculateResult() override
  {
    MOZ_ASSERT(!NS_IsMainThread());

    nsCString userNickname;
    CopyUTF16toUTF8(mResult.mNickname, userNickname);

    // Delete server certificate.
    nsCString serverCertName("WIFI_SERVERCERT_", 16);
    serverCertName += userNickname;
    nsresult rv = deleteCert(serverCertName);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Delete user certificate and private key.
    nsCString userCertName("WIFI_USERCERT_", 14);
    userCertName += userNickname;
    rv = deleteCert(userCertName);
    if (NS_FAILED(rv)) {
      return rv;
    }

    return NS_OK;
  }

  nsresult deleteCert(const nsCString &aCertNickname)
  {
    ScopedCERTCertificate cert(
      CERT_FindCertByNickname(CERT_GetDefaultCertDB(), aCertNickname.get())
    );
    // Because we delete certificates in blind, so it's acceptable to delete
    // a non-exist certificate.
    if (!cert) {
      return NS_OK;
    }

    ScopedPK11SlotInfo slot(
      PK11_KeyForCertExists(cert, nullptr, nullptr)
    );

    SECStatus srv;
    if (slot) {
      // Delete private key along with certificate.
      srv = PK11_DeleteTokenCertAndKey(cert, nullptr);
    } else {
      srv = SEC_DeletePermCertificate(cert);
    }

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

  mListener = aListener;

  return NS_OK;
}

NS_IMETHODIMP
WifiCertService::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());

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

NS_IMETHODIMP
WifiCertService::HasPrivateKey(const nsAString& aCertNickname, bool *aHasKey)
{
  *aHasKey = false;

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCString certNickname;
  CopyUTF16toUTF8(aCertNickname, certNickname);

  ScopedCERTCertificate cert(
    CERT_FindCertByNickname(CERT_GetDefaultCertDB(), certNickname.get())
  );
  if (!cert) {
    return NS_OK;
  }

  ScopedPK11SlotInfo slot(
    PK11_KeyForCertExists(cert, nullptr, nullptr)
  );
  if (slot) {
    *aHasKey = true;
  }

  return NS_OK;
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
