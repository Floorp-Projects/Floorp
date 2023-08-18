/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORM_WMF_MFCONTENTPROTECTIONMANAGER_H
#define DOM_MEDIA_PLATFORM_WMF_MFCONTENTPROTECTIONMANAGER_H

#include <mfapi.h>
#include <mfidl.h>
#include <windows.media.protection.h>
#include <wrl.h>

#include "MFCDMProxy.h"

namespace mozilla {

/**
 * MFContentProtectionManager is used to enable the encrypted playback for the
 * media engine.
 * https://docs.microsoft.com/en-us/windows/win32/api/mfidl/nn-mfidl-imfcontentprotectionmanager
 * https://docs.microsoft.com/en-us/uwp/api/windows.media.protection.mediaprotectionmanager
 */
class MFContentProtectionManager
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::RuntimeClassType::WinRtClassicComMix |
              Microsoft::WRL::RuntimeClassType::InhibitRoOriginateError>,
          IMFContentProtectionManager,
          ABI::Windows::Media::Protection::IMediaProtectionManager> {
 public:
  MFContentProtectionManager();
  ~MFContentProtectionManager();

  HRESULT RuntimeClassInitialize();

  void Shutdown();

  // IMFContentProtectionManager.
  IFACEMETHODIMP BeginEnableContent(IMFActivate* aEnablerActivate,
                                    IMFTopology* aTopology,
                                    IMFAsyncCallback* aCallback,
                                    IUnknown* aState) override;
  IFACEMETHODIMP EndEnableContent(IMFAsyncResult* aAsyncResult) override;

  // IMediaProtectionManager.
  // MFMediaEngine can query this interface to invoke get_Properties().
  IFACEMETHODIMP add_ServiceRequested(
      ABI::Windows::Media::Protection::IServiceRequestedEventHandler* aHandler,
      EventRegistrationToken* aCookie) override;
  IFACEMETHODIMP remove_ServiceRequested(
      EventRegistrationToken aCookie) override;
  IFACEMETHODIMP add_RebootNeeded(
      ABI::Windows::Media::Protection::IRebootNeededEventHandler* aHandler,
      EventRegistrationToken* aCookie) override;
  IFACEMETHODIMP remove_RebootNeeded(EventRegistrationToken aCookie) override;
  IFACEMETHODIMP add_ComponentLoadFailed(
      ABI::Windows::Media::Protection::IComponentLoadFailedEventHandler*
          aHandler,
      EventRegistrationToken* aCookie) override;
  IFACEMETHODIMP remove_ComponentLoadFailed(
      EventRegistrationToken aCookie) override;
  IFACEMETHODIMP get_Properties(
      ABI::Windows::Foundation::Collections::IPropertySet** aValue) override;

  HRESULT SetCDMProxy(MFCDMProxy* aCDMProxy);

  MFCDMProxy* GetCDMProxy() const { return mCDMProxy; }

 private:
  HRESULT SetPMPServer(
      ABI::Windows::Media::Protection::IMediaProtectionPMPServer* aPMPServer);

  RefPtr<MFCDMProxy> mCDMProxy;

  Microsoft::WRL::ComPtr<ABI::Windows::Foundation::Collections::IPropertySet>
      mPMPServerSet;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_PLATFORM_WMF_MFCONTENTPROTECTIONMANAGER_H
