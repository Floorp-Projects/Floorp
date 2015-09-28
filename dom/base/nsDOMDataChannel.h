/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMDataChannel_h
#define nsDOMDataChannel_h

#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/DataChannelBinding.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/net/DataChannelListener.h"
#include "nsIDOMDataChannel.h"
#include "nsIInputStream.h"


namespace mozilla {
namespace dom {
class Blob;
}

class DataChannel;
};

class nsDOMDataChannel final : public mozilla::DOMEventTargetHelper,
                               public nsIDOMDataChannel,
                               public mozilla::DataChannelListener
{
public:
  nsDOMDataChannel(already_AddRefed<mozilla::DataChannel>& aDataChannel,
                   nsPIDOMWindow* aWindow);

  nsresult Init(nsPIDOMWindow* aDOMWindow);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMDATACHANNEL

  NS_REALLY_FORWARD_NSIDOMEVENTTARGET(mozilla::DOMEventTargetHelper)

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsDOMDataChannel,
                                           mozilla::DOMEventTargetHelper)

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
    override;
  nsPIDOMWindow* GetParentObject() const
  {
    return GetOwner();
  }

  // WebIDL
  // Uses XPIDL GetLabel.
  bool Reliable() const;
  mozilla::dom::RTCDataChannelState ReadyState() const;
  uint32_t BufferedAmount() const;
  uint32_t BufferedAmountLowThreshold() const;
  void SetBufferedAmountLowThreshold(uint32_t aThreshold);
  IMPL_EVENT_HANDLER(open)
  IMPL_EVENT_HANDLER(error)
  IMPL_EVENT_HANDLER(close)
  // Uses XPIDL Close.
  IMPL_EVENT_HANDLER(message)
  IMPL_EVENT_HANDLER(bufferedamountlow)
  mozilla::dom::RTCDataChannelType BinaryType() const
  {
    return static_cast<mozilla::dom::RTCDataChannelType>(
      static_cast<int>(mBinaryType));
  }
  void SetBinaryType(mozilla::dom::RTCDataChannelType aType)
  {
    mBinaryType = static_cast<DataChannelBinaryType>(
      static_cast<int>(aType));
  }
  void Send(const nsAString& aData, mozilla::ErrorResult& aRv);
  void Send(mozilla::dom::Blob& aData, mozilla::ErrorResult& aRv);
  void Send(const mozilla::dom::ArrayBuffer& aData, mozilla::ErrorResult& aRv);
  void Send(const mozilla::dom::ArrayBufferView& aData,
            mozilla::ErrorResult& aRv);

  // Uses XPIDL GetProtocol.
  bool Ordered() const;
  uint16_t Id() const;
  uint16_t Stream() const; // deprecated

  nsresult
  DoOnMessageAvailable(const nsACString& aMessage, bool aBinary);

  virtual nsresult
  OnMessageAvailable(nsISupports* aContext, const nsACString& aMessage) override;

  virtual nsresult
  OnBinaryMessageAvailable(nsISupports* aContext, const nsACString& aMessage) override;

  virtual nsresult OnSimpleEvent(nsISupports* aContext, const nsAString& aName);

  virtual nsresult
  OnChannelConnected(nsISupports* aContext) override;

  virtual nsresult
  OnChannelClosed(nsISupports* aContext) override;

  virtual nsresult
  OnBufferLow(nsISupports* aContext) override;

  virtual void
  AppReady();

protected:
  ~nsDOMDataChannel();

private:
  void Send(nsIInputStream* aMsgStream, const nsACString& aMsgString,
            uint32_t aMsgLength, bool aIsBinary, mozilla::ErrorResult& aRv);

  // Owning reference
  nsRefPtr<mozilla::DataChannel> mDataChannel;
  nsString  mOrigin;
  enum DataChannelBinaryType {
    DC_BINARY_TYPE_ARRAYBUFFER,
    DC_BINARY_TYPE_BLOB,
  };
  DataChannelBinaryType mBinaryType;
};

#endif // nsDOMDataChannel_h
