/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WebSocket_h__
#define WebSocket_h__

#include "mozilla/Attributes.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/dom/WebSocketBinding.h" // for BinaryType
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/ErrorResult.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupports.h"
#include "nsISupportsUtils.h"
#include "nsString.h"
#include "nsWrapperCache.h"

#define DEFAULT_WS_SCHEME_PORT  80
#define DEFAULT_WSS_SCHEME_PORT 443

class nsIInputStream;

namespace mozilla {
namespace dom {

class File;

class WebSocketImpl;

class WebSocket final : public DOMEventTargetHelper
{
  friend class WebSocketImpl;

public:
  enum {
    CONNECTING = 0,
    OPEN       = 1,
    CLOSING    = 2,
    CLOSED     = 3
  };

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS_INHERITED(
    WebSocket, DOMEventTargetHelper)

  // EventTarget
  virtual void EventListenerAdded(nsIAtom* aType) override;
  virtual void EventListenerRemoved(nsIAtom* aType) override;

  virtual void DisconnectFromOwner() override;

  // nsWrapperCache
  nsPIDOMWindow* GetParentObject() { return GetOwner(); }

  virtual JSObject* WrapObject(JSContext* cx, JS::Handle<JSObject*> aGivenProto) override;

public: // static helpers:

  // Determine if preferences allow WebSocket
  static bool PrefEnabled(JSContext* aCx = nullptr, JSObject* aGlobal = nullptr);

public: // WebIDL interface:

  // Constructor:
  static already_AddRefed<WebSocket> Constructor(const GlobalObject& aGlobal,
                                                 const nsAString& aUrl,
                                                 ErrorResult& rv);

  static already_AddRefed<WebSocket> Constructor(const GlobalObject& aGlobal,
                                                 const nsAString& aUrl,
                                                 const nsAString& aProtocol,
                                                 ErrorResult& rv);

  static already_AddRefed<WebSocket> Constructor(const GlobalObject& aGlobal,
                                                 const nsAString& aUrl,
                                                 const Sequence<nsString>& aProtocols,
                                                 ErrorResult& rv);

  // webIDL: readonly attribute DOMString url
  void GetUrl(nsAString& aResult);

  // webIDL: readonly attribute unsigned short readyState;
  uint16_t ReadyState();

  // webIDL: readonly attribute unsigned long bufferedAmount;
  uint32_t BufferedAmount() const;

  // webIDL: attribute Function? onopen;
  IMPL_EVENT_HANDLER(open)

  // webIDL: attribute Function? onerror;
  IMPL_EVENT_HANDLER(error)

  // webIDL: attribute Function? onclose;
  IMPL_EVENT_HANDLER(close)

  // webIDL: readonly attribute DOMString extensions;
  void GetExtensions(nsAString& aResult);

  // webIDL: readonly attribute DOMString protocol;
  void GetProtocol(nsAString& aResult);

  // webIDL: void close(optional unsigned short code, optional DOMString reason):
  void Close(const Optional<uint16_t>& aCode,
             const Optional<nsAString>& aReason,
             ErrorResult& aRv);

  // webIDL: attribute Function? onmessage;
  IMPL_EVENT_HANDLER(message)

  // webIDL: attribute DOMString binaryType;
  dom::BinaryType BinaryType() const;
  void SetBinaryType(dom::BinaryType aData);

  // webIDL: void send(DOMString|Blob|ArrayBufferView data);
  void Send(const nsAString& aData,
            ErrorResult& aRv);
  void Send(File& aData,
            ErrorResult& aRv);
  void Send(const ArrayBuffer& aData,
            ErrorResult& aRv);
  void Send(const ArrayBufferView& aData,
            ErrorResult& aRv);

private: // constructor && distructor
  explicit WebSocket(nsPIDOMWindow* aOwnerWindow);
  virtual ~WebSocket();

  void SetReadyState(uint16_t aReadyState);

  // These methods actually do the dispatch for various events.
  nsresult CreateAndDispatchSimpleEvent(const nsAString& aName);
  nsresult CreateAndDispatchMessageEvent(const nsACString& aData,
                                         bool aIsBinary);
  nsresult CreateAndDispatchMessageEvent(JSContext* aCx,
                                         const nsACString& aData,
                                         bool aIsBinary);
  nsresult CreateAndDispatchCloseEvent(bool aWasClean,
                                       uint16_t aCode,
                                       const nsAString& aReason);

  // if there are "strong event listeners" (see comment in WebSocket.cpp) or
  // outgoing not sent messages then this method keeps the object alive
  // when js doesn't have strong references to it.
  void UpdateMustKeepAlive();
  // ATTENTION, when calling this method the object can be released
  // (and possibly collected).
  void DontKeepAliveAnyMore();

private:
  WebSocket(const WebSocket& x) = delete;   // prevent bad usage
  WebSocket& operator=(const WebSocket& x) = delete;

  void Send(nsIInputStream* aMsgStream,
            const nsACString& aMsgString,
            uint32_t aMsgLength,
            bool aIsBinary,
            ErrorResult& aRv);

  void AssertIsOnTargetThread() const;

  // Raw pointer because this WebSocketImpl is created, managed and destroyed by
  // WebSocket.
  WebSocketImpl* mImpl;

  bool mIsMainThread;

  bool mKeepingAlive;
  bool mCheckMustKeepAlive;

  uint32_t mOutgoingBufferedAmount;

  // related to the WebSocket constructor steps
  nsString mURI;
  nsString mEffectiveURL;   // after redirects
  nsCString mEstablishedExtensions;
  nsCString mEstablishedProtocol;

  dom::BinaryType mBinaryType;

  // This mutex protects mReadyState that is the only variable that is used in
  // different threads.
  mozilla::Mutex mMutex;

  // This value should not be used directly but use ReadyState() instead.
  uint16_t mReadyState;
};

} //namespace dom
} //namespace mozilla

#endif
