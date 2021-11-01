/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RTCDTMFSender.h"
#include "libwebrtcglue/MediaConduitInterface.h"
#include "transport/logging.h"
#include "TransceiverImpl.h"
#include "nsITimer.h"
#include "mozilla/dom/RTCDTMFSenderBinding.h"
#include "mozilla/dom/RTCDTMFToneChangeEvent.h"
#include <algorithm>
#include <bitset>

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(RTCDTMFSender, DOMEventTargetHelper,
                                   mTransceiver, mSendTimer)

NS_IMPL_ADDREF_INHERITED(RTCDTMFSender, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(RTCDTMFSender, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(RTCDTMFSender)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
  NS_INTERFACE_MAP_ENTRY(nsINamed)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

LazyLogModule gDtmfLog("RTCDTMFSender");

RTCDTMFSender::RTCDTMFSender(nsPIDOMWindowInner* aWindow,
                             TransceiverImpl* aTransceiver)
    : DOMEventTargetHelper(aWindow), mTransceiver(aTransceiver) {}

JSObject* RTCDTMFSender::WrapObject(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return RTCDTMFSender_Binding::Wrap(aCx, this, aGivenProto);
}

static int GetDTMFToneCode(uint16_t c) {
  const char* DTMF_TONECODES = "0123456789*#ABCD";

  if (c == ',') {
    // , is a special character indicating a 2 second delay
    return -1;
  }

  const char* i = strchr(DTMF_TONECODES, c);
  MOZ_ASSERT(i);
  return static_cast<int>(i - DTMF_TONECODES);
}

static std::bitset<256> GetCharacterBitset(const std::string& aCharsInSet) {
  std::bitset<256> result;
  for (auto c : aCharsInSet) {
    result[c] = true;
  }
  return result;
}

static bool IsUnrecognizedChar(const char c) {
  static const std::bitset<256> recognized =
      GetCharacterBitset("0123456789ABCD#*,");
  return !recognized[c];
}

void RTCDTMFSender::SetPayloadType(int32_t aPayloadType,
                                   int32_t aPayloadFrequency) {
  MOZ_ASSERT(NS_IsMainThread());
  mPayloadType = Some(aPayloadType);
  mPayloadFrequency = Some(aPayloadFrequency);
}

void RTCDTMFSender::InsertDTMF(const nsAString& aTones, uint32_t aDuration,
                               uint32_t aInterToneGap, ErrorResult& aRv) {
  if (!mTransceiver->CanSendDTMF()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  std::string utf8Tones = NS_ConvertUTF16toUTF8(aTones).get();

  std::transform(utf8Tones.begin(), utf8Tones.end(), utf8Tones.begin(),
                 [](const char c) { return std::toupper(c); });

  if (std::any_of(utf8Tones.begin(), utf8Tones.end(), IsUnrecognizedChar)) {
    aRv.Throw(NS_ERROR_DOM_INVALID_CHARACTER_ERR);
    return;
  }

  CopyUTF8toUTF16(utf8Tones, mToneBuffer);
  mDuration = std::clamp(aDuration, 40U, 6000U);
  mInterToneGap = std::clamp(aInterToneGap, 30U, 6000U);

  if (mToneBuffer.Length()) {
    StartPlayout(0);
  }
}

void RTCDTMFSender::StopPlayout() {
  if (mSendTimer) {
    mSendTimer->Cancel();
    mSendTimer = nullptr;
  }
}

void RTCDTMFSender::StartPlayout(uint32_t aDelay) {
  if (!mSendTimer) {
    mSendTimer = NS_NewTimer();
    mSendTimer->InitWithCallback(this, aDelay, nsITimer::TYPE_ONE_SHOT);
  }
}

nsresult RTCDTMFSender::Notify(nsITimer* timer) {
  MOZ_ASSERT(NS_IsMainThread());
  StopPlayout();

  if (!mTransceiver->IsSending()) {
    return NS_OK;
  }

  RTCDTMFToneChangeEventInit init;
  if (!mToneBuffer.IsEmpty()) {
    uint16_t toneChar = mToneBuffer.CharAt(0);
    int tone = GetDTMFToneCode(toneChar);

    init.mTone.Assign(toneChar);

    mToneBuffer.Cut(0, 1);

    if (tone == -1) {
      StartPlayout(2000);
    } else {
      // Reset delay if necessary
      StartPlayout(mDuration + mInterToneGap);
      mDtmfEvent.Notify(DtmfEvent(mPayloadType.ref(), mPayloadFrequency.ref(),
                                  tone, mDuration));
    }
  }

  RefPtr<RTCDTMFToneChangeEvent> event =
      RTCDTMFToneChangeEvent::Constructor(this, u"tonechange"_ns, init);
  DispatchTrustedEvent(event);

  return NS_OK;
}

nsresult RTCDTMFSender::GetName(nsACString& aName) {
  aName.AssignLiteral("RTCDTMFSender");
  return NS_OK;
}

void RTCDTMFSender::GetToneBuffer(nsAString& aOutToneBuffer) {
  aOutToneBuffer = mToneBuffer;
}

}  // namespace mozilla::dom

#undef LOGTAG
