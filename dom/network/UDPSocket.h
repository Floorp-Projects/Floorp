/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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

//
// set MOZ_LOG=UDPSocket:5
//

namespace mozilla {
namespace net {
extern LazyLogModule gUDPSocketLog;
#define UDPSOCKET_LOG(args) MOZ_LOG(gUDPSocketLog, LogLevel::Debug, args)
#define UDPSOCKET_LOG_ENABLED() MOZ_LOG_TEST(gUDPSocketLog, LogLevel::Debug)
}  // namespace net

namespace dom {

struct UDPOptions;
class StringOrBlobOrArrayBufferOrArrayBufferView;
class UDPSocketChild;

class UDPSocket final : public DOMEventTargetHelper,
                        public nsIUDPSocketListener,
                        public nsIUDPSocketInternal {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(UDPSocket, DOMEventTargetHelper)
  NS_DECL_NSIUDPSOCKETLISTENER
  NS_DECL_NSIUDPSOCKETINTERNAL

 public:
  nsPIDOMWindowInner* GetParentObject() const { return GetOwner(); }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  virtual void DisconnectFromOwner() override;

  static already_AddRefed<UDPSocket> Constructor(const GlobalObject& aGlobal,
                                                 const UDPOptions& aOptions,
                                                 ErrorResult& aRv);

  void GetLocalAddress(nsString& aRetVal) const { aRetVal = mLocalAddress; }

  Nullable<uint16_t> GetLocalPort() const { return mLocalPort; }

  void GetRemoteAddress(nsString& aRetVal) const {
    if (mRemoteAddress.IsVoid()) {
      SetDOMStringToNull(aRetVal);
      return;
    }

    aRetVal = NS_ConvertUTF8toUTF16(mRemoteAddress);
  }

  Nullable<uint16_t> GetRemotePort() const { return mRemotePort; }

  bool AddressReuse() const { return mAddressReuse; }

  bool Loopback() const { return mLoopback; }

  SocketReadyState ReadyState() const { return mReadyState; }

  Promise* Opened() const { return mOpened; }

  Promise* Closed() const { return mClosed; }

  IMPL_EVENT_HANDLER(message)

  already_AddRefed<Promise> Close();

  void JoinMulticastGroup(const nsAString& aMulticastGroupAddress,
                          ErrorResult& aRv);

  void LeaveMulticastGroup(const nsAString& aMulticastGroupAddress,
                           ErrorResult& aRv);

  bool Send(const StringOrBlobOrArrayBufferOrArrayBufferView& aData,
            const Optional<nsAString>& aRemoteAddress,
            const Optional<Nullable<uint16_t>>& aRemotePort, ErrorResult& aRv);

 private:
  class ListenerProxy : public nsIUDPSocketListener,
                        public nsIUDPSocketInternal {
   public:
    NS_DECL_ISUPPORTS
    NS_FORWARD_SAFE_NSIUDPSOCKETLISTENER(mSocket)
    NS_FORWARD_SAFE_NSIUDPSOCKETINTERNAL(mSocket)

    explicit ListenerProxy(UDPSocket* aSocket) : mSocket(aSocket) {}

    void Disconnect() { mSocket = nullptr; }

   private:
    virtual ~ListenerProxy() {}

    UDPSocket* mSocket;
  };

  UDPSocket(nsPIDOMWindowInner* aOwner, const nsCString& aRemoteAddress,
            const Nullable<uint16_t>& aRemotePort);

  virtual ~UDPSocket();

  nsresult Init(const nsString& aLocalAddress,
                const Nullable<uint16_t>& aLocalPort, const bool& aAddressReuse,
                const bool& aLoopback);

  nsresult InitLocal(const nsAString& aLocalAddress,
                     const uint16_t& aLocalPort);

  nsresult InitRemote(const nsAString& aLocalAddress,
                      const uint16_t& aLocalPort);

  void HandleReceivedData(const nsACString& aRemoteAddress,
                          const uint16_t& aRemotePort,
                          const nsTArray<uint8_t>& aData);

  nsresult DispatchReceivedData(const nsACString& aRemoteAddress,
                                const uint16_t& aRemotePort,
                                const nsTArray<uint8_t>& aData);

  void CloseWithReason(nsresult aReason);

  nsresult DoPendingMcastCommand();

  nsString mLocalAddress;
  Nullable<uint16_t> mLocalPort;
  nsCString mRemoteAddress;
  Nullable<uint16_t> mRemotePort;
  bool mAddressReuse;
  bool mLoopback;
  SocketReadyState mReadyState;
  RefPtr<Promise> mOpened;
  RefPtr<Promise> mClosed;

  nsCOMPtr<nsIUDPSocket> mSocket;
  RefPtr<UDPSocketChild> mSocketChild;
  RefPtr<ListenerProxy> mListenerProxy;

  struct MulticastCommand {
    enum CommandType { Join, Leave };

    MulticastCommand(CommandType aCommand, const nsAString& aAddress)
        : mCommand(aCommand), mAddress(aAddress) {}

    CommandType mCommand;
    nsString mAddress;
  };

  nsTArray<MulticastCommand> mPendingMcastCommands;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_UDPSocket_h__
