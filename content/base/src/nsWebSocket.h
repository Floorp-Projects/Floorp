/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Wellington Fernando de Macedo.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Wellington Fernando de Macedo <wfernandom2004@gmail.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

class nsWSCloseEvent;
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
friend class nsWSCloseEvent;
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
  void     FailConnectionQuietly();
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
  nsresult CreateAndDispatchSimpleEvent(const nsString& aName);
  nsresult CreateAndDispatchMessageEvent(const nsACString& aData,
                                         bool isBinary);
  nsresult CreateAndDispatchCloseEvent(bool aWasClean, PRUint16 aCode,
                                       const nsString &aReason);
  nsresult CreateResponseBlob(const nsACString& aData, JSContext *aCx,
                              jsval &jsData);

  void SetReadyState(PRUint16 aNewReadyState);

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
  bool mTriggeredCloseEvent;
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
