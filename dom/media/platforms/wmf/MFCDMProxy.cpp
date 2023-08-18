/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MFCDMProxy.h"

#include "MFCDMParent.h"
#include "MFMediaEngineUtils.h"

namespace mozilla {

using Microsoft::WRL::ComPtr;

#define LOG(msg, ...)                         \
  MOZ_LOG(gMFMediaEngineLog, LogLevel::Debug, \
          ("MFCDMProxy=%p, " msg, this, ##__VA_ARGS__))

MFCDMProxy::MFCDMProxy(IMFContentDecryptionModule* aCDM, uint64_t aCDMParentId)
    : mCDM(aCDM), mCDMParentId(aCDMParentId) {
  LOG("MFCDMProxy created, created by %" PRId64 " MFCDMParent", mCDMParentId);
}

MFCDMProxy::~MFCDMProxy() { LOG("MFCDMProxy destroyed"); }

void MFCDMProxy::Shutdown() {
  if (mTrustedInput) {
    mTrustedInput = nullptr;
  }
  for (auto& inputAuthorities : mInputTrustAuthorities) {
    SHUTDOWN_IF_POSSIBLE(inputAuthorities.second);
  }
  mInputTrustAuthorities.clear();
  if (auto* parent = MFCDMParent::GetCDMById(mCDMParentId)) {
    parent->ShutdownCDM();
  }
  mCDM = nullptr;
  LOG("MFCDMProxy Shutdowned");
}

HRESULT MFCDMProxy::GetPMPServer(REFIID aRiid, LPVOID* aPMPServerOut) {
  ComPtr<IMFGetService> cdmServices;
  RETURN_IF_FAILED(mCDM.As(&cdmServices));
  RETURN_IF_FAILED(cdmServices->GetService(MF_CONTENTDECRYPTIONMODULE_SERVICE,
                                           aRiid, aPMPServerOut));
  return S_OK;
}

HRESULT MFCDMProxy::GetInputTrustAuthority(uint32_t aStreamId,
                                           const uint8_t* aContentInitData,
                                           uint32_t aContentInitDataSize,
                                           REFIID aRiid,
                                           IUnknown** aInputTrustAuthorityOut) {
  if (mInputTrustAuthorities.count(aStreamId)) {
    RETURN_IF_FAILED(
        mInputTrustAuthorities[aStreamId].CopyTo(aInputTrustAuthorityOut));
    return S_OK;
  }

  if (!mTrustedInput) {
    RETURN_IF_FAILED(mCDM->CreateTrustedInput(
        aContentInitData, aContentInitDataSize, &mTrustedInput));
    LOG("Created a trust input for stream %u", aStreamId);
  }

  // GetInputTrustAuthority takes IUnknown* as the output. Using other COM
  // interface will have a v-table mismatch issue.
  ComPtr<IUnknown> unknown;
  RETURN_IF_FAILED(
      mTrustedInput->GetInputTrustAuthority(aStreamId, aRiid, &unknown));

  ComPtr<IMFInputTrustAuthority> inputTrustAuthority;
  RETURN_IF_FAILED(unknown.As(&inputTrustAuthority));
  RETURN_IF_FAILED(unknown.CopyTo(aInputTrustAuthorityOut));

  mInputTrustAuthorities[aStreamId] = inputTrustAuthority;
  return S_OK;
}

HRESULT MFCDMProxy::SetContentEnabler(IUnknown* aRequest,
                                      IMFAsyncResult* aResult) {
  LOG("SetContentEnabler");
  ComPtr<IMFContentEnabler> contentEnabler;
  RETURN_IF_FAILED(aRequest->QueryInterface(IID_PPV_ARGS(&contentEnabler)));
  return mCDM->SetContentEnabler(contentEnabler.Get(), aResult);
}

void MFCDMProxy::OnHardwareContextReset() {
  LOG("OnHardwareContextReset");
  // Hardware context reset happens, all the crypto sessions are in invalid
  // states. So drop everything here.
  mTrustedInput.Reset();
  mInputTrustAuthorities.clear();
}

#undef LOG

}  // namespace mozilla
