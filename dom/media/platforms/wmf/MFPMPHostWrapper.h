/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORM_WMF_MPMPHOSTWRAPPER_H
#define DOM_MEDIA_PLATFORM_WMF_MPMPHOSTWRAPPER_H

#include <wrl.h>
#include <wrl/client.h>

#include "MFCDMExtra.h"

namespace mozilla {

// This class is used to create and manage PMP sessions. For PlayReady CDM,
// it needs to connect with IMFPMPHostApp first before generating any request.
// That behavior is undocumented on the mdsn, see more details in
// https://github.com/microsoft/media-foundation/issues/37#issuecomment-1197321484
class MFPMPHostWrapper : public Microsoft::WRL::RuntimeClass<
                             Microsoft::WRL::RuntimeClassFlags<
                                 Microsoft::WRL::RuntimeClassType::ClassicCom>,
                             IMFPMPHostApp> {
 public:
  MFPMPHostWrapper();
  ~MFPMPHostWrapper();

  HRESULT RuntimeClassInitialize(Microsoft::WRL::ComPtr<IMFPMPHost>& aHost);

  STDMETHODIMP LockProcess() override;

  STDMETHODIMP UnlockProcess() override;

  STDMETHODIMP ActivateClassById(LPCWSTR aId, IStream* aStream, REFIID aRiid,
                                 void** aActivatedClass) override;

  void Shutdown();

 private:
  Microsoft::WRL::ComPtr<IMFPMPHost> mPMPHost;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_PLATFORM_WMF_MPMPHOSTWRAPPER_H
