/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MFMediaEngineNotify.h"

#include "MFMediaEngineUtils.h"

namespace mozilla {

IFACEMETHODIMP MFMediaEngineNotify::EventNotify(DWORD aEvent, DWORD_PTR aParam1,
                                                DWORD aParam2) {
  auto event = static_cast<MF_MEDIA_ENGINE_EVENT>(aEvent);
  MFMediaEngineEventWrapper engineEvent{event};
  if (event == MF_MEDIA_ENGINE_EVENT_ERROR ||
      event == MF_MEDIA_ENGINE_EVENT_FORMATCHANGE ||
      event == MF_MEDIA_ENGINE_EVENT_NOTIFYSTABLESTATE) {
    engineEvent.mParam1 = Some(aParam1);
    engineEvent.mParam2 = Some(aParam2);
  }
  mEngineEvents.Notify(engineEvent);
  return S_OK;
}

}  // namespace mozilla
