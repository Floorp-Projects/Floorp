/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MFContentProtectionManager.h"

#include <winnt.h>

#include "MFMediaEngineUtils.h"
#include "WMF.h"
#include "WMFUtils.h"

namespace mozilla {

using Microsoft::WRL::ComPtr;

#define LOG(msg, ...)                         \
  MOZ_LOG(gMFMediaEngineLog, LogLevel::Debug, \
          ("MFContentProtectionManager=%p, " msg, this, ##__VA_ARGS__))

MFContentProtectionManager::MFContentProtectionManager() {
  MOZ_COUNT_CTOR(MFContentProtectionManager);
  LOG("MFContentProtectionManager created");
}

MFContentProtectionManager::~MFContentProtectionManager() {
  MOZ_COUNT_DTOR(MFContentProtectionManager);
  LOG("MFContentProtectionManager destroyed");
}

HRESULT MFContentProtectionManager::RuntimeClassInitialize() {
  ScopedHString propertyId(
      RuntimeClass_Windows_Foundation_Collections_PropertySet);
  RETURN_IF_FAILED(RoActivateInstance(propertyId.Get(), &mPMPServerSet));
  return S_OK;
}

HRESULT MFContentProtectionManager::BeginEnableContent(
    IMFActivate* aEnablerActivate, IMFTopology* aTopology,
    IMFAsyncCallback* aCallback, IUnknown* aState) {
  LOG("BeginEnableContent");
  ComPtr<IUnknown> unknownObject;
  ComPtr<IMFAsyncResult> asyncResult;
  RETURN_IF_FAILED(
      wmf::MFCreateAsyncResult(nullptr, aCallback, aState, &asyncResult));
  RETURN_IF_FAILED(
      aEnablerActivate->ActivateObject(IID_PPV_ARGS(&unknownObject)));

  GUID enablerType = GUID_NULL;
  ComPtr<IMFContentEnabler> contentEnabler;
  if (SUCCEEDED(unknownObject.As(&contentEnabler))) {
    RETURN_IF_FAILED(contentEnabler->GetEnableType(&enablerType));
  } else {
    ComPtr<ABI::Windows::Media::Protection::IMediaProtectionServiceRequest>
        serviceRequest;
    RETURN_IF_FAILED(unknownObject.As(&serviceRequest));
    RETURN_IF_FAILED(serviceRequest->get_Type(&enablerType));
  }

  if (enablerType == MFENABLETYPE_MF_RebootRequired) {
    LOG("Error - MFENABLETYPE_MF_RebootRequired");
    return MF_E_REBOOT_REQUIRED;
  } else if (enablerType == MFENABLETYPE_MF_UpdateRevocationInformation) {
    LOG("Error - MFENABLETYPE_MF_UpdateRevocationInformation");
    return MF_E_GRL_VERSION_TOO_LOW;
  } else if (enablerType == MFENABLETYPE_MF_UpdateUntrustedComponent) {
    LOG("Error - MFENABLETYPE_MF_UpdateUntrustedComponent");
    return HRESULT_FROM_WIN32(ERROR_INVALID_IMAGE_HASH);
  }

  if (!mCDMProxy) {
    return MF_E_SHUTDOWN;
  }
  RETURN_IF_FAILED(
      mCDMProxy->SetContentEnabler(unknownObject.Get(), asyncResult.Get()));

  // TODO : maybe need to notify waiting for key status?
  LOG("Finished BeginEnableContent");
  return S_OK;
}

HRESULT MFContentProtectionManager::EndEnableContent(
    IMFAsyncResult* aAsyncResult) {
  HRESULT hr = aAsyncResult->GetStatus();
  if (FAILED(hr)) {
    // Follow Chromium to not to return failure, which avoid doing additional
    // work here.
    LOG("Content enabling failed. hr=%lx", hr);
  } else {
    LOG("Content enabling succeeded");
  }
  return S_OK;
}

HRESULT MFContentProtectionManager::add_ServiceRequested(
    ABI::Windows::Media::Protection::IServiceRequestedEventHandler* aHandler,
    EventRegistrationToken* aCookie) {
  return E_NOTIMPL;
}

HRESULT MFContentProtectionManager::remove_ServiceRequested(
    EventRegistrationToken aCookie) {
  return E_NOTIMPL;
}

HRESULT MFContentProtectionManager::add_RebootNeeded(
    ABI::Windows::Media::Protection::IRebootNeededEventHandler* aHandler,
    EventRegistrationToken* aCookie) {
  return E_NOTIMPL;
}

HRESULT MFContentProtectionManager::remove_RebootNeeded(
    EventRegistrationToken aCookie) {
  return E_NOTIMPL;
}

HRESULT MFContentProtectionManager::add_ComponentLoadFailed(
    ABI::Windows::Media::Protection::IComponentLoadFailedEventHandler* aHandler,
    EventRegistrationToken* aCookie) {
  return E_NOTIMPL;
}

HRESULT MFContentProtectionManager::remove_ComponentLoadFailed(
    EventRegistrationToken aCookie) {
  return E_NOTIMPL;
}

HRESULT MFContentProtectionManager::get_Properties(
    ABI::Windows::Foundation::Collections::IPropertySet** properties) {
  if (!mPMPServerSet) {
    return E_POINTER;
  }
  return mPMPServerSet.CopyTo(properties);
}

HRESULT MFContentProtectionManager::SetCDMProxy(MFCDMProxy* aCDMProxy) {
  MOZ_ASSERT(aCDMProxy);
  mCDMProxy = aCDMProxy;
  ComPtr<ABI::Windows::Media::Protection::IMediaProtectionPMPServer> pmpServer;
  RETURN_IF_FAILED(mCDMProxy->GetPMPServer(IID_PPV_ARGS(&pmpServer)));
  RETURN_IF_FAILED(SetPMPServer(pmpServer.Get()));
  return S_OK;
}

HRESULT MFContentProtectionManager::SetPMPServer(
    ABI::Windows::Media::Protection::IMediaProtectionPMPServer* aPMPServer) {
  MOZ_ASSERT(aPMPServer);

  ComPtr<ABI::Windows::Foundation::Collections::IMap<HSTRING, IInspectable*>>
      serverMap;
  RETURN_IF_FAILED(mPMPServerSet.As(&serverMap));

  // MFMediaEngine uses |serverKey| to get the Protected Media Path (PMP)
  // server used for playing protected content. This is not currently documented
  // in MSDN.
  boolean replaced = false;
  ScopedHString serverKey{L"Windows.Media.Protection.MediaProtectionPMPServer"};
  RETURN_IF_FAILED(serverMap->Insert(serverKey.Get(), aPMPServer, &replaced));
  return S_OK;
}

void MFContentProtectionManager::Shutdown() {
  if (mCDMProxy) {
    mCDMProxy->Shutdown();
    mCDMProxy = nullptr;
  }
}

#undef LOG

}  // namespace mozilla
