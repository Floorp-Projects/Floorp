/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
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
#include "nsIInterfaceRequestor.h"
#include "nsIObserver.h"
#include "nsIRequest.h"
#include "nsISupports.h"
#include "nsISupportsUtils.h"
#include "nsIWebSocketChannel.h"
#include "nsIWebSocketListener.h"
#include "nsString.h"
#include "nsWeakReference.h"
#include "nsWrapperCache.h"

#define DEFAULT_WS_SCHEME_PORT  80
#define DEFAULT_WSS_SCHEME_PORT 443

namespace mozilla {
namespace dom {

class WebSocket MOZ_FINAL : public DOMEventTargetHelper,
                            public nsIInterfaceRequestor,
                            public nsIWebSocketListener,
                            public nsIObserver,
                            public nsSupportsWeakReference,
                            public nsIRequest
{
friend class CallDispatchConnectionCloseEvents;
friend class nsAutoCloseWS;

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
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSIWEBSOCKETLISTENER
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIREQUEST

  // EventTarget
  virtual void EventListenerAdded(nsIAtom* aType) MOZ_OVERRIDE;
  virtual void EventListenerRemoved(nsIAtom* aType) MOZ_OVERRIDE;

  virtual void DisconnectFromOwner() MOZ_OVERRIDE;

  // nsWrapperCache
  nsPIDOMWindow* GetParentObject() { return GetOwner(); }

  virtual JSObject* WrapObject(JSContext *cx) MOZ_OVERRIDE;

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
  uint16_t ReadyState() const { return mReadyState; }

  // webIDL: readonly attribute unsigned long bufferedAmount;
  uint32_t BufferedAmount() const { return mOutgoingBufferedAmount; }

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
  dom::BinaryType BinaryType() const { return mBinaryType; }
  void SetBinaryType(dom::BinaryType aData) { mBinaryType = aData; }

  // webIDL: void send(DOMString|Blob|ArrayBufferView data);
  void Send(const nsAString& aData,
            ErrorResult& aRv);
  void Send(nsIDOMBlob* aData,
            ErrorResult& aRv);
  void Send(const ArrayBuffer& aData,
            ErrorResult& aRv);
  void Send(const ArrayBufferView& aData,
            ErrorResult& aRv);

private: // constructor && distructor
  explicit WebSocket(nsPIDOMWindow* aOwnerWindow);
  virtual ~WebSocket();

protected:
  nsresult Init(JSContext* aCx,
                nsIPrincipal* aPrincipal,
                const nsAString& aURL,
                nsTArray<nsString>& aProtocolArray);

  void Send(nsIInputStream* aMsgStream,
            const nsACString& aMsgString,
            uint32_t aMsgLength,
            bool aIsBinary,
            ErrorResult& aRv);

  nsresult ParseURL(const nsString& aURL);
  nsresult EstablishConnection();

  // These methods when called can release the WebSocket object
  void FailConnection(uint16_t reasonCode,
                      const nsACString& aReasonString = EmptyCString());
  nsresult CloseConnection(uint16_t reasonCode,
                           const nsACString& aReasonString = EmptyCString());
  nsresult Disconnect();

  nsresult ConsoleError();
  nsresult PrintErrorOnConsole(const char* aBundleURI,
                               const char16_t* aError,
                               const char16_t** aFormatStrings,
                               uint32_t aFormatStringsLen);

  nsresult DoOnMessageAvailable(const nsACString& aMsg,
                                bool isBinary);

  // ConnectionCloseEvents: 'error' event if needed, then 'close' event.
  // - These must not be dispatched while we are still within an incoming call
  //   from JS (ex: close()).  Set 'sync' to false in that case to dispatch in a
  //   separate new event.
  nsresult ScheduleConnectionCloseEvents(nsISupports* aContext,
                                         nsresult aStatusCode,
                                         bool sync);
  // 2nd half of ScheduleConnectionCloseEvents, sometimes run in its own event.
  void DispatchConnectionCloseEvents();

  // These methods actually do the dispatch for various events.
  nsresult CreateAndDispatchSimpleEvent(const nsString& aName);
  nsresult CreateAndDispatchMessageEvent(const nsACString& aData,
                                         bool isBinary);
  nsresult CreateAndDispatchCloseEvent(bool aWasClean,
                                       uint16_t aCode,
                                       const nsString& aReason);

  // if there are "strong event listeners" (see comment in WebSocket.cpp) or
  // outgoing not sent messages then this method keeps the object alive
  // when js doesn't have strong references to it.
  void UpdateMustKeepAlive();
  // ATTENTION, when calling this method the object can be released
  // (and possibly collected).
  void DontKeepAliveAnyMore();

  nsresult UpdateURI();

protected: //data

  nsCOMPtr<nsIWebSocketChannel> mChannel;

  // related to the WebSocket constructor steps
  nsString mOriginalURL;
  nsString mEffectiveURL;   // after redirects
  bool mSecure; // if true it is using SSL and the wss scheme,
                // otherwise it is using the ws scheme with no SSL

  bool mKeepingAlive;
  bool mCheckMustKeepAlive;
  bool mOnCloseScheduled;
  bool mFailed;
  bool mDisconnected;

  // Set attributes of DOM 'onclose' message
  bool      mCloseEventWasClean;
  nsString  mCloseEventReason;
  uint16_t  mCloseEventCode;

  nsCString mAsciiHost;  // hostname
  uint32_t  mPort;
  nsCString mResource; // [filepath[?query]]
  nsString  mUTF16Origin;

  nsCOMPtr<nsIURI> mURI;
  nsCString mRequestedProtocolList;
  nsCString mEstablishedProtocol;
  nsCString mEstablishedExtensions;

  uint16_t mReadyState;

  nsCOMPtr<nsIPrincipal> mPrincipal;

  uint32_t mOutgoingBufferedAmount;

  dom::BinaryType mBinaryType;

  // Web Socket owner information:
  // - the script file name, UTF8 encoded.
  // - source code line number where the Web Socket object was constructed.
  // - the ID of the inner window where the script lives. Note that this may not
  //   be the same as the Web Socket owner window.
  // These attributes are used for error reporting.
  nsCString mScriptFile;
  uint32_t mScriptLine;
  uint64_t mInnerWindowID;

private:
  WebSocket(const WebSocket& x) MOZ_DELETE;   // prevent bad usage
  WebSocket& operator=(const WebSocket& x) MOZ_DELETE;
};

} //namespace dom
} //namespace mozilla

#endif
