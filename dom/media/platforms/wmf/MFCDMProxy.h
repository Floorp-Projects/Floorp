/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORM_WMF_MFCDMPROXY_H
#define DOM_MEDIA_PLATFORM_WMF_MFCDMPROXY_H

#include <map>
#include <mfobjects.h>
#include <unknwn.h>
#include <windef.h>
#include <wrl.h>

#include "MFCDMExtra.h"
#include "nsISupportsImpl.h"

namespace mozilla {

/**
 * MFCDMProxy wraps a IMFContentDecryptionModule and provides some high level
 * helper methods in order to allow caller to interact with the wrapped CDM.
 */
class MFCDMProxy {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MFCDMProxy);

  MFCDMProxy(IMFContentDecryptionModule* aCDM, uint64_t aCDMParentId);

 public:
  // Return a IMediaProtectionPMPServer from the existing CDM.
  HRESULT GetPMPServer(REFIID aRiid, LPVOID* aPMPServerOut);

  // Return a IMFInputTrustAuthority for given stream id, the same stream ID
  // always maps to the same IMFInputTrustAuthority. In addition,
  // `aContentInitData` is optional initialization data as in
  // https://www.w3.org/TR/encrypted-media/#initialization-data
  HRESULT GetInputTrustAuthority(uint32_t aStreamId,
                                 const uint8_t* aContentInitData,
                                 uint32_t aContentInitDataSize, REFIID aRiid,
                                 IUnknown** aInputTrustAuthorityOut);

  // Set IMFContentEnabler to the existing CDM, `aRequest` should be a inherited
  // class of `IMFContentEnabler`.
  HRESULT SetContentEnabler(IUnknown* aRequest, IMFAsyncResult* aResult);

  // Notify the CDM on DRM_E_TEE_INVALID_HWDRM_STATE (0x8004cd12), which happens
  // in cases like OS Sleep. In this case, the CDM should close all sessions
  // because they are in bad state.
  void OnHardwareContextReset();

  void Shutdown();

  // TODO : set last key id in order to let CDM use the key IDs information to
  // perform some optimization.

 private:
  ~MFCDMProxy();

  Microsoft::WRL::ComPtr<IMFContentDecryptionModule> mCDM;

  // The same ITA is always mapping to the same stream Id.
  std::map<uint32_t /* stream Id */,
           Microsoft::WRL::ComPtr<IMFInputTrustAuthority>>
      mInputTrustAuthorities;

  Microsoft::WRL::ComPtr<IMFTrustedInput> mTrustedInput;

  const uint64_t mCDMParentId;

  // TODO : need some events? (Eg. significant playback, error, hardware context
  // reset)
};

}  // namespace mozilla

#endif  // DOM_MEDIA_PLATFORM_WMF_MFCDMPROXY_H
