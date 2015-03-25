/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TCPServerSocket_h
#define mozilla_dom_TCPServerSocket_h

#include "mozilla/DOMEventTargetHelper.h"
#include "nsIServerSocket.h"

namespace mozilla {
class ErrorResult;
namespace dom {

struct ServerSocketOptions;
class GlobalObject;
class TCPSocket;

class TCPServerSocket final : public DOMEventTargetHelper
                            , public nsIServerSocketListener
{
public:
  TCPServerSocket(nsIGlobalObject* aGlobal, uint16_t aPort, bool aUseArrayBuffers,
                  uint16_t aBacklog);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(TCPServerSocket, DOMEventTargetHelper)
  NS_DECL_NSISERVERSOCKETLISTENER

  nsPIDOMWindow* GetParentObject() const
  {
    return GetOwner();
  }

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  uint16_t LocalPort();
  void Close();

  static already_AddRefed<TCPServerSocket>
  Constructor(const GlobalObject& aGlobal,
              uint16_t aPort,
              const ServerSocketOptions& aOptions,
              uint16_t aBacklog,
              mozilla::ErrorResult& aRv);

  IMPL_EVENT_HANDLER(connect);
  IMPL_EVENT_HANDLER(error);

private:
  ~TCPServerSocket();
  nsresult Init();
  // Dispatch a TCPServerSocketEvent event of a given type at this object.
  void FireEvent(const nsAString& aType, TCPSocket* aSocket);

  // The server socket associated with this object.
  nsCOMPtr<nsIServerSocket> mServerSocket;
  int32_t mPort;
  uint16_t mBacklog;
  // True if any accepted sockets should use array buffers for received messages.
  bool mUseArrayBuffers;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TCPServerSocket_h
