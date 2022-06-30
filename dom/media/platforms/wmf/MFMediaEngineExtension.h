/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORM_WMF_MFMEDIAENGINEEXTENSION_H
#define DOM_MEDIA_PLATFORM_WMF_MFMEDIAENGINEEXTENSION_H

#include <wrl.h>

#include "MFMediaEngineExtra.h"

namespace mozilla {

/**
 * MFMediaEngineNotify is used to load media resources in the media engine.
 * https://docs.microsoft.com/en-us/windows/win32/api/mfmediaengine/nn-mfmediaengine-imfmediaengineextension
 */
class MFMediaEngineExtension final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::RuntimeClassType::ClassicCom>,
          IMFMediaEngineExtension> {
 public:
  MFMediaEngineExtension() = default;

  HRESULT RuntimeClassInitialize() { return S_OK; }

  void SetMediaSource(IMFMediaSource* aMediaSource);

  // Method for MFMediaEngineExtension
  IFACEMETHODIMP BeginCreateObject(BSTR aUrl, IMFByteStream* aByteStream,
                                   MF_OBJECT_TYPE aType,
                                   IUnknown** aCancelCookie,
                                   IMFAsyncCallback* aCallback,
                                   IUnknown* aState) override;
  IFACEMETHODIMP CancelObjectCreation(IUnknown* aCancelCookie) override;
  IFACEMETHODIMP EndCreateObject(IMFAsyncResult* aResult,
                                 IUnknown** aRetObj) override;
  IFACEMETHODIMP CanPlayType(BOOL aIsAudioOnly, BSTR aMimeType,
                             MF_MEDIA_ENGINE_CANPLAY* aResult) override;

 private:
  bool mIsObjectCreating = false;
  Microsoft::WRL::ComPtr<IMFMediaSource> mMediaSource;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_PLATFORM_WMF_MFMEDIAENGINEEXTENSION_H
