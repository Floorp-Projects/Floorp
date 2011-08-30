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
#include "nsIMozWebSocket.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIJSNativeInitializer.h"
#include "nsIPrincipal.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIDOMEventListener.h"
#include "nsDOMEventTargetWrapperCache.h"
#include "nsAutoPtr.h"
#include "nsIDOMDOMStringList.h"

#define DEFAULT_WS_SCHEME_PORT  80
#define DEFAULT_WSS_SCHEME_PORT 443

#define NS_WEBSOCKET_CID                            \
 { /* 7ca25214-98dc-40a6-bc1f-41ddbe41f46c */       \
  0x7ca25214, 0x98dc, 0x40a6,                       \
 {0xbc, 0x1f, 0x41, 0xdd, 0xbe, 0x41, 0xf4, 0x6c} }

#define NS_WEBSOCKET_CONTRACTID "@mozilla.org/websocket;1"

class nsWSNetAddressComparator;
class nsWebSocketEstablishedConnection;
class nsWSCloseEvent;

class nsWebSocket: public nsDOMEventTargetWrapperCache,
                   public nsIMozWebSocket,
                   public nsIJSNativeInitializer
{
friend class nsWSNetAddressComparator;
friend class nsWebSocketEstablishedConnection;
friend class nsWSCloseEvent;

public:
  nsWebSocket();
  virtual ~nsWebSocket();
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsWebSocket,
                                           nsDOMEventTargetWrapperCache)
  NS_DECL_NSIMOZWEBSOCKET

  // nsIJSNativeInitializer
  NS_IMETHOD Initialize(nsISupports* aOwner, JSContext* aContext,
                        JSObject* aObject, PRUint32 aArgc, jsval* aArgv);

  // nsIDOMEventTarget
  NS_IMETHOD AddEventListener(const nsAString& aType,
                              nsIDOMEventListener *aListener,
                              PRBool aUseCapture,
                              PRBool aWantsUntrusted,
                              PRUint8 optional_argc);
  NS_IMETHOD RemoveEventListener(const nsAString& aType,
                                 nsIDOMEventListener* aListener,
                                 PRBool aUseCapture);

  // Determine if preferences allow WebSocket
  static PRBool PrefEnabled();

  const PRUint64 InnerWindowID() const { return mInnerWindowID; }
  const nsCString& GetScriptFile() const { return mScriptFile; }
  const PRUint32 GetScriptLine() const { return mScriptLine; }

protected:
  nsresult ParseURL(const nsString& aURL);
  nsresult EstablishConnection();

  nsresult CreateAndDispatchSimpleEvent(const nsString& aName);
  nsresult CreateAndDispatchMessageEvent(const nsACString& aData);
  nsresult CreateAndDispatchCloseEvent(PRBool aWasClean, PRUint16 aCode,
                                       const nsString &aReason);

  // called from mConnection accordingly to the situation
  void SetReadyState(PRUint16 aNewReadyState);

  // if there are "strong event listeners" (see comment in nsWebSocket.cpp) or
  // outgoing not sent messages then this method keeps the object alive
  // when js doesn't have strong references to it.
  void UpdateMustKeepAlive();
  // ATTENTION, when calling this method the object can be released
  // (and possibly collected).
  void DontKeepAliveAnyMore();

  nsRefPtr<nsDOMEventListenerWrapper> mOnOpenListener;
  nsRefPtr<nsDOMEventListenerWrapper> mOnErrorListener;
  nsRefPtr<nsDOMEventListenerWrapper> mOnMessageListener;
  nsRefPtr<nsDOMEventListenerWrapper> mOnCloseListener;

  // related to the WebSocket constructor steps
  nsString mOriginalURL;
  PRPackedBool mSecure; // if true it is using SSL and the wss scheme,
                        // otherwise it is using the ws scheme with no SSL

  PRPackedBool mKeepingAlive;
  PRPackedBool mCheckMustKeepAlive;
  PRPackedBool mTriggeredCloseEvent;

  nsCString mClientReason;
  PRUint16  mClientReasonCode;
  nsString  mServerReason;
  PRUint16  mServerReasonCode;

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

  nsRefPtr<nsWebSocketEstablishedConnection> mConnection;
  PRUint32 mOutgoingBufferedAmount; // actually, we get this value from
                                    // mConnection when we are connected,
                                    // but we need this one after disconnecting.

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
