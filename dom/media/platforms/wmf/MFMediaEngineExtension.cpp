/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MFMediaEngineExtension.h"

#include "MFMediaSource.h"
#include "MFMediaEngineUtils.h"

namespace mozilla {

#define LOG(msg, ...)                         \
  MOZ_LOG(gMFMediaEngineLog, LogLevel::Debug, \
          ("MFMediaEngineExtension=%p, " msg, this, ##__VA_ARGS__))

using Microsoft::WRL::ComPtr;

void MFMediaEngineExtension::SetMediaSource(IMFMediaSource* aMediaSource) {
  LOG("SetMediaSource=%p", aMediaSource);
  mMediaSource = aMediaSource;
}

// https://docs.microsoft.com/en-us/windows/win32/api/mfmediaengine/nf-mfmediaengine-imfmediaengineextension-begincreateobject
IFACEMETHODIMP MFMediaEngineExtension::BeginCreateObject(
    BSTR aUrl, IMFByteStream* aByteStream, MF_OBJECT_TYPE aType,
    IUnknown** aCancelCookie, IMFAsyncCallback* aCallback, IUnknown* aState) {
  if (aCancelCookie) {
    // We don't support a cancel cookie.
    *aCancelCookie = nullptr;
  }

  if (aType != MF_OBJECT_MEDIASOURCE) {
    LOG("Only support media source type");
    return MF_E_UNEXPECTED;
  }

  MOZ_ASSERT(mMediaSource);
  ComPtr<IMFAsyncResult> result;
  ComPtr<IUnknown> sourceUnknown = mMediaSource;
  RETURN_IF_FAILED(wmf::MFCreateAsyncResult(sourceUnknown.Get(), aCallback,
                                            aState, &result));
  RETURN_IF_FAILED(result->SetStatus(S_OK));

  LOG("Creating object");
  mIsObjectCreating = true;

  RETURN_IF_FAILED(aCallback->Invoke(result.Get()));
  return S_OK;
}

IFACEMETHODIMP MFMediaEngineExtension::CancelObjectCreation(
    IUnknown* aCancelCookie) {
  return MF_E_UNEXPECTED;
}

IFACEMETHODIMP MFMediaEngineExtension::EndCreateObject(IMFAsyncResult* aResult,
                                                       IUnknown** aRetObj) {
  *aRetObj = nullptr;
  if (!mIsObjectCreating) {
    LOG("No object is creating, not an expected call");
    return MF_E_UNEXPECTED;
  }

  RETURN_IF_FAILED(aResult->GetStatus());
  RETURN_IF_FAILED(aResult->GetObject(aRetObj));

  LOG("End of creating object");
  mIsObjectCreating = false;
  return S_OK;
}

IFACEMETHODIMP MFMediaEngineExtension::CanPlayType(
    BOOL aIsAudioOnly, BSTR aMimeType, MF_MEDIA_ENGINE_CANPLAY* aResult) {
  // We use MF_MEDIA_ENGINE_EXTENSION to resolve as custom media source for
  // MFMediaEngine, MIME types are not used.
  *aResult = MF_MEDIA_ENGINE_CANPLAY_NOT_SUPPORTED;
  return S_OK;
}

// TODO : break cycle of mMediaSource

#undef LOG

}  // namespace mozilla
