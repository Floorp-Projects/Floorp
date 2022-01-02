/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _RTCDTMFSender_h_
#define _RTCDTMFSender_h_

#include "MediaEventSource.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/RefPtr.h"
#include "js/RootingAPI.h"
#include "nsITimer.h"

class nsPIDOMWindowInner;
class nsITimer;

namespace mozilla {
class AudioSessionConduit;
class TransceiverImpl;

struct DtmfEvent {
  DtmfEvent(int aPayloadType, int aPayloadFrequency, int aEventCode,
            int aLengthMs)
      : mPayloadType(aPayloadType),
        mPayloadFrequency(aPayloadFrequency),
        mEventCode(aEventCode),
        mLengthMs(aLengthMs) {}
  const int mPayloadType;
  const int mPayloadFrequency;
  const int mEventCode;
  const int mLengthMs;
};

namespace dom {

class RTCDTMFSender : public DOMEventTargetHelper,
                      public nsITimerCallback,
                      public nsINamed {
 public:
  RTCDTMFSender(nsPIDOMWindowInner* aWindow, TransceiverImpl* aTransceiver);

  // nsISupports
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSINAMED
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(RTCDTMFSender, DOMEventTargetHelper)

  // webidl
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;
  void SetPayloadType(int32_t aPayloadType, int32_t aPayloadFrequency);
  void InsertDTMF(const nsAString& aTones, uint32_t aDuration,
                  uint32_t aInterToneGap, ErrorResult& aRv);
  void GetToneBuffer(nsAString& aOutToneBuffer);
  IMPL_EVENT_HANDLER(tonechange)

  void StopPlayout();

  MediaEventSource<DtmfEvent>& OnDtmfEvent() { return mDtmfEvent; }

 private:
  virtual ~RTCDTMFSender() = default;

  void StartPlayout(uint32_t aDelay);

  RefPtr<TransceiverImpl> mTransceiver;
  MediaEventProducer<DtmfEvent> mDtmfEvent;
  Maybe<int32_t> mPayloadType;
  Maybe<int32_t> mPayloadFrequency;
  nsString mToneBuffer;
  uint32_t mDuration = 0;
  uint32_t mInterToneGap = 0;
  nsCOMPtr<nsITimer> mSendTimer;
};

}  // namespace dom
}  // namespace mozilla
#endif  // _RTCDTMFSender_h_
