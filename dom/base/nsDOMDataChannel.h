/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMDataChannel_h
#define nsDOMDataChannel_h

#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/RTCDataChannelBinding.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/net/DataChannelListener.h"

namespace mozilla {
namespace dom {
class Blob;
}

class DataChannel;
};  // namespace mozilla

class nsDOMDataChannel final : public mozilla::DOMEventTargetHelper,
                               public mozilla::DataChannelListener {
 public:
  nsDOMDataChannel(already_AddRefed<mozilla::DataChannel>& aDataChannel,
                   nsPIDOMWindowInner* aWindow);

  nsresult Init(nsPIDOMWindowInner* aDOMWindow);

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsDOMDataChannel,
                                           mozilla::DOMEventTargetHelper)

  // EventTarget
  using EventTarget::EventListenerAdded;
  virtual void EventListenerAdded(nsAtom* aType) override;

  using EventTarget::EventListenerRemoved;
  virtual void EventListenerRemoved(nsAtom* aType) override;

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;
  nsPIDOMWindowInner* GetParentObject() const { return GetOwner(); }

  // WebIDL
  void GetLabel(nsAString& aLabel);
  void GetProtocol(nsAString& aProtocol);
  bool Reliable() const;
  mozilla::dom::Nullable<uint16_t> GetMaxPacketLifeTime() const;
  mozilla::dom::Nullable<uint16_t> GetMaxRetransmits() const;
  mozilla::dom::RTCDataChannelState ReadyState() const;
  uint32_t BufferedAmount() const;
  uint32_t BufferedAmountLowThreshold() const;
  void SetBufferedAmountLowThreshold(uint32_t aThreshold);
  IMPL_EVENT_HANDLER(open)
  IMPL_EVENT_HANDLER(error)
  IMPL_EVENT_HANDLER(close)
  void Close();
  IMPL_EVENT_HANDLER(message)
  IMPL_EVENT_HANDLER(bufferedamountlow)
  mozilla::dom::RTCDataChannelType BinaryType() const {
    return static_cast<mozilla::dom::RTCDataChannelType>(
        static_cast<int>(mBinaryType));
  }
  void SetBinaryType(mozilla::dom::RTCDataChannelType aType) {
    mBinaryType = static_cast<DataChannelBinaryType>(static_cast<int>(aType));
  }
  void Send(const nsAString& aData, mozilla::ErrorResult& aRv);
  void Send(mozilla::dom::Blob& aData, mozilla::ErrorResult& aRv);
  void Send(const mozilla::dom::ArrayBuffer& aData, mozilla::ErrorResult& aRv);
  void Send(const mozilla::dom::ArrayBufferView& aData,
            mozilla::ErrorResult& aRv);

  bool Negotiated() const;
  bool Ordered() const;
  mozilla::dom::Nullable<uint16_t> GetId() const;

  nsresult DoOnMessageAvailable(const nsACString& aMessage, bool aBinary);

  virtual nsresult OnMessageAvailable(nsISupports* aContext,
                                      const nsACString& aMessage) override;

  virtual nsresult OnBinaryMessageAvailable(
      nsISupports* aContext, const nsACString& aMessage) override;

  virtual nsresult OnSimpleEvent(nsISupports* aContext, const nsAString& aName);

  virtual nsresult OnChannelConnected(nsISupports* aContext) override;

  virtual nsresult OnChannelClosed(nsISupports* aContext) override;

  virtual nsresult OnBufferLow(nsISupports* aContext) override;

  virtual nsresult NotBuffered(nsISupports* aContext) override;

  // if there are "strong event listeners" or outgoing not sent messages
  // then this method keeps the object alive when js doesn't have strong
  // references to it.
  void UpdateMustKeepAlive();
  // ATTENTION, when calling this method the object can be released
  // (and possibly collected).
  void DontKeepAliveAnyMore();

 protected:
  ~nsDOMDataChannel();

 private:
  void Send(mozilla::dom::Blob* aMsgBlob, const nsACString* aMsgString,
            bool aIsBinary, mozilla::ErrorResult& aRv);

  void ReleaseSelf();

  // to keep us alive while we have listeners
  RefPtr<nsDOMDataChannel> mSelfRef;
  // Owning reference
  RefPtr<mozilla::DataChannel> mDataChannel;
  nsString mOrigin;
  enum DataChannelBinaryType {
    DC_BINARY_TYPE_ARRAYBUFFER,
    DC_BINARY_TYPE_BLOB,
  };
  DataChannelBinaryType mBinaryType;
  bool mCheckMustKeepAlive;
  bool mSentClose;
};

#endif  // nsDOMDataChannel_h
