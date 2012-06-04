/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsWebSocket_h__
#define nsWebSocket_h__

#include "nsISupportsUtils.h"
#include "nsIWebSocket.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIJSNativeInitializer.h"
#include "nsIPrincipal.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIDOMEventListener.h"
#include "nsDOMEventTargetHelper.h"
#include "nsAutoPtr.h"
#include "nsIDOMDOMStringList.h"
#include "nsIInterfaceRequestor.h"
#include "nsIWebSocketChannel.h"
#include "nsIWebSocketListener.h"
#include "nsIObserver.h"
#include "nsIRequest.h"
#include "nsWeakReference.h"

#define DEFAULT_WS_SCHEME_PORT  80
#define DEFAULT_WSS_SCHEME_PORT 443

#define NS_WEBSOCKET_CID                            \
 { /* 7ca25214-98dc-40a6-bc1f-41ddbe41f46c */       \
  0x7ca25214, 0x98dc, 0x40a6,                       \
 {0xbc, 0x1f, 0x41, 0xdd, 0xbe, 0x41, 0xf4, 0x6c} }

#define NS_WEBSOCKET_CONTRACTID "@mozilla.org/websocket;1"

class CallDispatchConnectionCloseEvents;
class nsAutoCloseWS;

class nsWebSocket: public nsDOMEventTargetHelper,
                   public nsIWebSocket,
                   public nsIJSNativeInitializer,
                   public nsIInterfaceRequestor,
                   public nsIWebSocketListener,
                   public nsIObserver,
                   public nsSupportsWeakReference,
                   public nsIRequest
{
friend class CallDispatchConnectionCloseEvents;
friend class nsAutoCloseWS;

public:
  nsWebSocket();
  virtual ~nsWebSocket();
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS_INHERITED(nsWebSocket,
                                                                   nsDOMEventTargetHelper)
  NS_DECL_NSIWEBSOCKET
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSIWEBSOCKETLISTENER
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIREQUEST

  // nsIJSNativeInitializer
  NS_IMETHOD Initialize(nsISupports* aOwner, JSContext* aContext,
                        JSObject* aObject, PRUint32 aArgc, jsval* aArgv);

  // nsIDOMEventTarget
  NS_IMETHOD AddEventListener(const nsAString& aType,
                              nsIDOMEventListener *aListener,
                              bool aUseCapture,
                              bool aWantsUntrusted,
                              PRUint8 optional_argc);
  NS_IMETHOD RemoveEventListener(const nsAString& aType,
                                 nsIDOMEventListener* aListener,
                                 bool aUseCapture);

  // Determine if preferences allow WebSocket
  static bool PrefEnabled();

  virtual void DisconnectFromOwner();
protected:
  nsresult ParseURL(const nsString& aURL);
  nsresult EstablishConnection();

  // These methods when called can release the WebSocket object
  nsresult FailConnection(PRUint16 reasonCode,
                          const nsACString& aReasonString = EmptyCString());
  nsresult CloseConnection(PRUint16 reasonCode,
                           const nsACString& aReasonString = EmptyCString());
  nsresult Disconnect();

  nsresult ConsoleError();
  nsresult PrintErrorOnConsole(const char       *aBundleURI,
                               const PRUnichar  *aError,
                               const PRUnichar **aFormatStrings,
                               PRUint32          aFormatStringsLen);

  nsresult ConvertTextToUTF8(const nsString& aMessage, nsCString& buf);

  // Get msg info out of JS variable being sent (string, arraybuffer, blob)
  nsresult GetSendParams(nsIVariant *aData, nsCString &aStringOut,
                         nsCOMPtr<nsIInputStream> &aStreamOut,
                         bool &aIsBinary, PRUint32 &aOutgoingLength,
                         JSContext *aCx);

  nsresult DoOnMessageAvailable(const nsACString & aMsg, bool isBinary);

  // ConnectionCloseEvents: 'error' event if needed, then 'close' event.
  // - These must not be dispatched while we are still within an incoming call
  //   from JS (ex: close()).  Set 'sync' to false in that case to dispatch in a
  //   separate new event.
  nsresult ScheduleConnectionCloseEvents(nsISupports *aContext,
                                         nsresult aStatusCode,
                                         bool sync);
  // 2nd half of ScheduleConnectionCloseEvents, sometimes run in its own event.
  void     DispatchConnectionCloseEvents();

  // These methods actually do the dispatch for various events.
  nsresult CreateAndDispatchSimpleEvent(const nsString& aName);
  nsresult CreateAndDispatchMessageEvent(const nsACString& aData,
                                         bool isBinary);
  nsresult CreateAndDispatchCloseEvent(bool aWasClean, PRUint16 aCode,
                                       const nsString &aReason);
  nsresult CreateResponseBlob(const nsACString& aData, JSContext *aCx,
                              jsval &jsData);

  // if there are "strong event listeners" (see comment in nsWebSocket.cpp) or
  // outgoing not sent messages then this method keeps the object alive
  // when js doesn't have strong references to it.
  void UpdateMustKeepAlive();
  // ATTENTION, when calling this method the object can be released
  // (and possibly collected).
  void DontKeepAliveAnyMore();

  nsresult UpdateURI();

  nsCOMPtr<nsIWebSocketChannel> mChannel;

  nsRefPtr<nsDOMEventListenerWrapper> mOnOpenListener;
  nsRefPtr<nsDOMEventListenerWrapper> mOnErrorListener;
  nsRefPtr<nsDOMEventListenerWrapper> mOnMessageListener;
  nsRefPtr<nsDOMEventListenerWrapper> mOnCloseListener;

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
  PRUint16  mCloseEventCode;

  nsCString mAsciiHost;  // hostname
  PRUint32  mPort;
  nsCString mResource; // [filepath[?query]]
  nsString  mUTF16Origin;

  nsCOMPtr<nsIURI> mURI;
  nsCString mRequestedProtocolList;
  nsCString mEstablishedProtocol;
  nsCString mEstablishedExtensions;

  PRUint16 mReadyState;

  nsCOMPtr<nsIPrincipal> mPrincipal;

  PRUint32 mOutgoingBufferedAmount;

  enum
  {
    WS_BINARY_TYPE_ARRAYBUFFER,
    WS_BINARY_TYPE_BLOB,
  } mBinaryType;

  // Web Socket owner information:
  // - the script file name, UTF8 encoded.
  // - source code line number where the Web Socket object was constructed.
  // - the ID of the inner window where the script lives. Note that this may not
  //   be the same as the Web Socket owner window.
  // These attributes are used for error reporting.
  nsCString mScriptFile;
  PRUint32 mScriptLine;
  PRUint64 mInnerWindowID;

private:
  nsWebSocket(const nsWebSocket& x);   // prevent bad usage
  nsWebSocket& operator=(const nsWebSocket& x);
};

#endif
