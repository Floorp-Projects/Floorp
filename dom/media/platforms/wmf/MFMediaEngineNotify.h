/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORM_WMF_MFMEDIAENGINENOTIFY_H
#define DOM_MEDIA_PLATFORM_WMF_MFMEDIAENGINENOTIFY_H

#include <wrl.h>

#include "MediaEventSource.h"
#include "MFMediaEngineExtra.h"
#include "mozilla/Maybe.h"

namespace mozilla {

const char* MediaEngineEventToStr(MF_MEDIA_ENGINE_EVENT aEvent);

// https://docs.microsoft.com/en-us/windows/win32/api/mfmediaengine/ne-mfmediaengine-mf_media_engine_event
struct MFMediaEngineEventWrapper final {
  explicit MFMediaEngineEventWrapper(MF_MEDIA_ENGINE_EVENT aEvent)
      : mEvent(aEvent) {}
  MF_MEDIA_ENGINE_EVENT mEvent;
  Maybe<DWORD_PTR> mParam1;
  Maybe<DWORD> mParam2;
};

/**
 * MFMediaEngineNotify is used to handle the event sent from the media engine.
 * https://docs.microsoft.com/en-us/windows/win32/api/mfmediaengine/nn-mfmediaengine-imfmediaenginenotify
 */
class MFMediaEngineNotify final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::RuntimeClassType::ClassicCom>,
          IMFMediaEngineNotify> {
 public:
  MFMediaEngineNotify() = default;

  HRESULT RuntimeClassInitialize() { return S_OK; }

  // Method for IMFMediaEngineNotify
  IFACEMETHODIMP EventNotify(DWORD aEvent, DWORD_PTR aParam1,
                             DWORD aParam2) override;

  MediaEventSource<MFMediaEngineEventWrapper>& MediaEngineEvent() {
    return mEngineEvents;
  }

 private:
  MediaEventProducer<MFMediaEngineEventWrapper> mEngineEvents;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_PLATFORM_WMF_MFMEDIAENGINENOTIFY_H
