/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_UDPSocket_h__
#define mozilla_dom_UDPSocket_h__

#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/SocketCommonBinding.h"
#include "nsIUDPSocket.h"
#include "nsIUDPSocketChild.h"
#include "nsTArray.h"

struct JSContext;

namespace mozilla {
namespace dom {

struct UDPOptions;
class StringOrBlobOrArrayBufferOrArrayBufferView;

class UDPSocket MOZ_FINAL : public DOMEventTargetHelper
                          , public nsIUDPSocketListener
                          , public nsIUDPSocketInternal
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(UDPSocket, DOMEventTargetHelper)
  NS_DECL_NSIUDPSOCKETLISTENER
  NS_DECL_NSIUDPSOCKETINTERNAL
  NS_REALLY_FORWARD_NSIDOMEVENTTARGET(DOMEventTargetHelper)

public:
  nsPIDOMWindow*
  GetParentObject() const
  {
    return GetOwner();
  }

  virtual JSObject*
  WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  virtual void
  DisconnectFromOwner() MOZ_OVERRIDE;

  static already_AddRefed<UDPSocket>
  Constructor(const GlobalObject& aGlobal, const UDPOptions& aOptions, ErrorResult& aRv);

  void
  GetLocalAddress(nsString& aRetVal) const
  {
    aRetVal = mLocalAddress;
  }

  Nullable<uint16_t>
  GetLocalPort() const
  {
    return mLocalPort;
  }

  void
  GetRemoteAddress(nsString& aRetVal) const
  {
    if (mRemoteAddress.IsVoid()) {
      SetDOMStringToNull(aRetVal);
      return;
    }

    aRetVal = NS_ConvertUTF8toUTF16(mRemoteAddress);
  }

  Nullable<uint16_t>
  GetRemotePort() const
  {
    return mRemotePort;
  }

  bool
  AddressReuse() const
  {
    return mAddressReuse;
  }

  bool
  Loopback() const
  {
    return mLoopback;
  }

  SocketReadyState
  ReadyState() const
  {
    return mReadyState;
  }

  Promise*
  Opened() const
  {
    return mOpened;
  }

  Promise*
  Closed() const
  {
    return mClosed;
  }

  IMPL_EVENT_HANDLER(message)

  already_AddRefed<Promise>
  Close();

  void
  JoinMulticastGroup(const nsAString& aMulticastGroupAddress, ErrorResult& aRv);

  void
  LeaveMulticastGroup(const nsAString& aMulticastGroupAddress, ErrorResult& aRv);

  bool
  Send(const StringOrBlobOrArrayBufferOrArrayBufferView& aData,
       const Optional<nsAString>& aRemoteAddress,
       const Optional<Nullable<uint16_t>>& aRemotePort,
       ErrorResult& aRv);

private:
  UDPSocket(nsPIDOMWindow* aOwner,
            const nsCString& aRemoteAddress,
            const Nullable<uint16_t>& aRemotePort);

  virtual ~UDPSocket();

  nsresult
  Init(const nsString& aLocalAddress,
       const Nullable<uint16_t>& aLocalPort,
       const bool& aAddressReuse,
       const bool& aLoopback);

  nsresult
  InitLocal(const nsAString& aLocalAddress, const uint16_t& aLocalPort);

  nsresult
  InitRemote(const nsAString& aLocalAddress, const uint16_t& aLocalPort);

  void
  HandleReceivedData(const nsACString& aRemoteAddress,
                     const uint16_t& aRemotePort,
                     const uint8_t* aData,
                     const uint32_t& aDataLength);

  nsresult
  DispatchReceivedData(const nsACString& aRemoteAddress,
                       const uint16_t& aRemotePort,
                       const uint8_t* aData,
                       const uint32_t& aDataLength);

  void
  CloseWithReason(nsresult aReason);

  nsresult
  DoPendingMcastCommand();

  nsString mLocalAddress;
  Nullable<uint16_t> mLocalPort;
  nsCString mRemoteAddress;
  Nullable<uint16_t> mRemotePort;
  bool mAddressReuse;
  bool mLoopback;
  SocketReadyState mReadyState;
  nsRefPtr<Promise> mOpened;
  nsRefPtr<Promise> mClosed;

  nsCOMPtr<nsIUDPSocket> mSocket;
  nsCOMPtr<nsIUDPSocketChild> mSocketChild;

  struct MulticastCommand {
    enum CommandType { Join, Leave };

    MulticastCommand(CommandType aCommand, const nsAString& aAddress)
      : mCommand(aCommand), mAddress(aAddress)
    { }

    CommandType mCommand;
    nsString mAddress;
  };

  nsTArray<MulticastCommand> mPendingMcastCommands;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_UDPSocket_h__
