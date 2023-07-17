/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MFPMPHostWrapper.h"

#include "MFMediaEngineUtils.h"
#include "WMF.h"
#include "mozilla/EMEUtils.h"

namespace mozilla {

using Microsoft::WRL::ComPtr;

#define LOG(msg, ...) EME_LOG("MFPMPHostWrapper=%p, " msg, this, ##__VA_ARGS__)

HRESULT MFPMPHostWrapper::RuntimeClassInitialize(
    Microsoft::WRL::ComPtr<IMFPMPHost>& aHost) {
  mPMPHost = aHost;
  return S_OK;
}

MFPMPHostWrapper::MFPMPHostWrapper() {
  MOZ_COUNT_CTOR(MFPMPHostWrapper);
  LOG("MFPMPHostWrapper created");
}

MFPMPHostWrapper::~MFPMPHostWrapper() {
  MOZ_COUNT_DTOR(MFPMPHostWrapper);
  LOG("MFPMPHostWrapper destroyed");
};

STDMETHODIMP MFPMPHostWrapper::LockProcess() {
  LOG("LockProcess");
  return mPMPHost->LockProcess();
}

STDMETHODIMP MFPMPHostWrapper::UnlockProcess() {
  LOG("UnlockProcess");
  return mPMPHost->UnlockProcess();
}

STDMETHODIMP MFPMPHostWrapper::ActivateClassById(LPCWSTR aId, IStream* aStream,
                                                 REFIID aRiid,
                                                 void** aActivatedClass) {
  LOG("ActivateClassById, id=%ls", aId);
  ComPtr<IMFAttributes> creationAttributes;
  RETURN_IF_FAILED(wmf::MFCreateAttributes(&creationAttributes, 2));
  RETURN_IF_FAILED(creationAttributes->SetString(GUID_ClassName, aId));

  if (aStream) {
    STATSTG statstg;
    RETURN_IF_FAILED(
        aStream->Stat(&statstg, STATFLAG_NOOPEN | STATFLAG_NONAME));
    nsTArray<uint8_t> streamBlob;
    streamBlob.SetLength(statstg.cbSize.LowPart);
    unsigned long readSize = 0;
    RETURN_IF_FAILED(
        aStream->Read(&streamBlob[0], streamBlob.Length(), &readSize));
    RETURN_IF_FAILED(creationAttributes->SetBlob(GUID_ObjectStream,
                                                 &streamBlob[0], readSize));
  }

  ComPtr<IStream> outputStream;
  RETURN_IF_FAILED(CreateStreamOnHGlobal(nullptr, TRUE, &outputStream));
  RETURN_IF_FAILED(wmf::MFSerializeAttributesToStream(creationAttributes.Get(),
                                                      0, outputStream.Get()));
  RETURN_IF_FAILED(outputStream->Seek({}, STREAM_SEEK_SET, nullptr));

  ComPtr<IMFActivate> activator;
  RETURN_IF_FAILED(mPMPHost->CreateObjectByCLSID(
      CLSID_EMEStoreActivate, outputStream.Get(), IID_PPV_ARGS(&activator)));
  RETURN_IF_FAILED(activator->ActivateObject(aRiid, aActivatedClass));
  LOG("Done ActivateClassById, id=%ls", aId);
  return S_OK;
}

void MFPMPHostWrapper::Shutdown() {
  LOG("Shutdown");
  if (mPMPHost) {
    mPMPHost = nullptr;
  }
}

#undef LOG

}  // namespace mozilla
