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
 *    Patrick McManus <mcmanus@ducksong.com>
 *    Jason Duell <jduell.mcbugs@gmail.com>
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

#include "mozilla/Util.h"

#include "nsWebSocket.h"

#include "nsIScriptGlobalObject.h"
#include "nsIDOMWindow.h"
#include "nsIDocument.h"
#include "nsXPCOM.h"
#include "nsIXPConnect.h"
#include "nsContentUtils.h"
#include "nsEventDispatcher.h"
#include "nsDOMError.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsDOMClassInfoID.h"
#include "jsapi.h"
#include "nsIURL.h"
#include "nsIPrivateDOMEvent.h"
#include "nsICharsetConverterManager.h"
#include "nsIUnicodeEncoder.h"
#include "nsThreadUtils.h"
#include "nsIDOMMessageEvent.h"
#include "nsIPromptFactory.h"
#include "nsIWindowWatcher.h"
#include "nsIPrompt.h"
#include "nsIStringBundle.h"
#include "nsIConsoleService.h"
#include "nsLayoutStatics.h"
#include "nsIDOMCloseEvent.h"
#include "nsICryptoHash.h"
#include "jsdbgapi.h"
#include "nsIJSContextStack.h"
#include "nsJSUtils.h"
#include "nsIScriptError.h"
#include "nsNetUtil.h"
#include "nsILoadGroup.h"
#include "mozilla/Preferences.h"
#include "nsDOMLists.h"
#include "xpcpublic.h"
#include "nsContentPolicyUtils.h"
#include "nsContentErrors.h"
#include "jstypedarray.h"
#include "prmem.h"
#include "nsDOMFile.h"

using namespace mozilla;

#define UTF_8_REPLACEMENT_CHAR    static_cast<PRUnichar>(0xFFFD)

#define TRUE_OR_FAIL_WEBSOCKET(x, ret)                                    \
  PR_BEGIN_MACRO                                                          \
    if (NS_UNLIKELY(!(x))) {                                              \
       NS_WARNING("ENSURE_TRUE_AND_FAIL_IF_FAILED(" #x ") failed");       \
       FailConnection();                                                  \
       return ret;                                                        \
    }                                                                     \
  PR_END_MACRO

#define SUCCESS_OR_FAIL_WEBSOCKET(res, ret)                               \
  PR_BEGIN_MACRO                                                          \
    nsresult __rv = res;                                                  \
    if (NS_FAILED(__rv)) {                                                \
      NS_ENSURE_SUCCESS_BODY(res, ret)                                    \
      FailConnection();                                                   \
      return ret;                                                         \
    }                                                                     \
  PR_END_MACRO

nsresult
nsWebSocket::PrintErrorOnConsole(const char *aBundleURI,
                                 const PRUnichar *aError,
                                 const PRUnichar **aFormatStrings,
                                 PRUint32 aFormatStringsLen)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");
  nsresult rv;

  nsCOMPtr<nsIStringBundleService> bundleService =
    do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIStringBundle> strBundle;
  rv = bundleService->CreateBundle(aBundleURI, getter_AddRefs(strBundle));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIConsoleService> console(
    do_GetService(NS_CONSOLESERVICE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIScriptError> errorObject(
    do_CreateInstance(NS_SCRIPTERROR_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  // Localize the error message
  nsXPIDLString message;
  if (aFormatStrings) {
    rv = strBundle->FormatStringFromName(aError, aFormatStrings,
                                         aFormatStringsLen,
                                         getter_Copies(message));
  } else {
    rv = strBundle->GetStringFromName(aError, getter_Copies(message));
  }
  NS_ENSURE_SUCCESS(rv, rv);

  rv = errorObject->InitWithWindowID(message.get(),
                                     NS_ConvertUTF8toUTF16(mScriptFile).get(),
                                     nsnull, mScriptLine, 0,
                                     nsIScriptError::errorFlag, "Web Socket",
                                     mInnerWindowID);
  NS_ENSURE_SUCCESS(rv, rv);

  // print the error message directly to the JS console
  rv = console->LogMessage(errorObject);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

// when this is called the browser side wants no more part of it
nsresult
nsWebSocket::CloseConnection()
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");
  if (mDisconnected)
    return NS_OK;

  // Disconnect() can release this object, so we keep a
  // reference until the end of the method
  nsRefPtr<nsWebSocket> kungfuDeathGrip = this;

  if (mReadyState == nsIWebSocket::CONNECTING) {
    SetReadyState(nsIWebSocket::CLOSED);
    if (mChannel) {
      mChannel->Close(mClientReasonCode, mClientReason);
    }
    Disconnect();
    return NS_OK;
  }

  SetReadyState(nsIWebSocket::CLOSING);

  if (mDisconnected) {
    SetReadyState(nsIWebSocket::CLOSED);
    Disconnect();
    return NS_OK;
  }

  return mChannel->Close(mClientReasonCode, mClientReason);
}

nsresult
nsWebSocket::ConsoleError()
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");
  nsresult rv;

  nsCAutoString targetSpec;
  rv = mURI->GetSpec(targetSpec);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to get targetSpec");
  } else {
    NS_ConvertUTF8toUTF16 specUTF16(targetSpec);
    const PRUnichar *formatStrings[] = { specUTF16.get() };

    if (mReadyState < nsIWebSocket::OPEN) {
      PrintErrorOnConsole("chrome://global/locale/appstrings.properties",
                          NS_LITERAL_STRING("connectionFailure").get(),
                          formatStrings, ArrayLength(formatStrings));
    } else {
      PrintErrorOnConsole("chrome://global/locale/appstrings.properties",
                          NS_LITERAL_STRING("netInterrupt").get(),
                          formatStrings, ArrayLength(formatStrings));
    }
  }
  /// todo some specific errors - like for message too large
  return rv;
}


nsresult
nsWebSocket::FailConnection()
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");
  ConsoleError();

  nsresult rv = CloseConnection();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = CreateAndDispatchSimpleEvent(NS_LITERAL_STRING("error"));
  if (NS_FAILED(rv))
    NS_WARNING("Failed to dispatch the error event");

  return NS_OK;
}

nsresult
nsWebSocket::Disconnect()
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");

  if (mDisconnected)
    return NS_OK;

  nsCOMPtr<nsILoadGroup> loadGroup;
  GetLoadGroup(getter_AddRefs(loadGroup));
  if (loadGroup)
    loadGroup->RemoveRequest(this, nsnull, NS_OK);

  // DontKeepAliveAnyMore() can release the object. So hold a reference to this
  // until the end of the method.
  nsRefPtr<nsWebSocket> kungfuDeathGrip = this;

  DontKeepAliveAnyMore();
  mChannel = nsnull;
  mDisconnected = true;

  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsWebSocket::nsIWebSocketListener methods:
//-----------------------------------------------------------------------------

nsresult
nsWebSocket::DoOnMessageAvailable(const nsACString & aMsg, bool isBinary)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");
  NS_ABORT_IF_FALSE(!mDisconnected, "Received message after disconnecting");

  if (mReadyState == nsIWebSocket::OPEN) {
    // Dispatch New Message
    nsresult rv = CreateAndDispatchMessageEvent(aMsg, isBinary);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to dispatch the message event");
    }
  } else {
    // CLOSING should be the only other state where it's possible to get msgs
    // from channel: Spec says to drop them.
    NS_ASSERTION(mReadyState == nsIWebSocket::CLOSING,
                 "Received message while CONNECTING or CLOSED");
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWebSocket::OnMessageAvailable(nsISupports *aContext, const nsACString & aMsg)
{
  return DoOnMessageAvailable(aMsg, false);
}

NS_IMETHODIMP
nsWebSocket::OnBinaryMessageAvailable(nsISupports *aContext,
                                      const nsACString & aMsg)
{
  return DoOnMessageAvailable(aMsg, true);
}

NS_IMETHODIMP
nsWebSocket::OnStart(nsISupports *aContext)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");
  if (mDisconnected)
    return NS_OK;

  if (!mRequestedProtocolList.IsEmpty()) {
    mChannel->GetProtocol(mEstablishedProtocol);
  }

  mChannel->GetExtensions(mEstablishedExtensions);
  UpdateURI();

  SetReadyState(nsIWebSocket::OPEN);
  return NS_OK;
}

NS_IMETHODIMP
nsWebSocket::OnStop(nsISupports *aContext, nsresult aStatusCode)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");
  if (mDisconnected)
    return NS_OK;

  mClosedCleanly = NS_SUCCEEDED(aStatusCode);

  if (aStatusCode == NS_BASE_STREAM_CLOSED &&
      mReadyState >= nsIWebSocket::CLOSING) {
    // don't generate an error event just because of an unclean close
    aStatusCode = NS_OK;
  }

  if (NS_FAILED(aStatusCode)) {
    ConsoleError();
    nsresult rv = CreateAndDispatchSimpleEvent(NS_LITERAL_STRING("error"));
    if (NS_FAILED(rv))
      NS_WARNING("Failed to dispatch the error event");
  }

  SetReadyState(nsIWebSocket::CLOSED);
  Disconnect();

  return NS_OK;
}

NS_IMETHODIMP
nsWebSocket::OnAcknowledge(nsISupports *aContext, PRUint32 aSize)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");

  if (aSize > mOutgoingBufferedAmount)
    return NS_ERROR_UNEXPECTED;

  mOutgoingBufferedAmount -= aSize;
  return NS_OK;
}

NS_IMETHODIMP
nsWebSocket::OnServerClose(nsISupports *aContext, PRUint16 aCode,
                           const nsACString &aReason)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");
  mServerReasonCode = aCode;
  CopyUTF8toUTF16(aReason, mServerReason);

  CloseConnection();                              /* reciprocate! */
  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsWebSocket::nsIInterfaceRequestor
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsWebSocket::GetInterface(const nsIID &aIID, void **aResult)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");

  if (mDisconnected)
    return NS_ERROR_FAILURE;

  if (aIID.Equals(NS_GET_IID(nsIAuthPrompt)) ||
      aIID.Equals(NS_GET_IID(nsIAuthPrompt2))) {
    nsresult rv;

    nsCOMPtr<nsIDocument> doc =
      nsContentUtils::GetDocumentFromScriptContext(mScriptContext);
    if (!doc)
      return NS_ERROR_NOT_AVAILABLE;

    nsCOMPtr<nsIPromptFactory> wwatch =
      do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsPIDOMWindow> outerWindow = doc->GetWindow();
    return wwatch->GetPrompt(outerWindow, aIID, aResult);
  }

  return NS_ERROR_UNEXPECTED;
}

////////////////////////////////////////////////////////////////////////////////
// nsWebSocket
////////////////////////////////////////////////////////////////////////////////

nsWebSocket::nsWebSocket() : mKeepingAlive(false),
                             mCheckMustKeepAlive(true),
                             mTriggeredCloseEvent(false),
                             mClosedCleanly(false),
                             mDisconnected(false),
                             mClientReasonCode(0),
                             mServerReasonCode(nsIWebSocketChannel::CLOSE_ABNORMAL),
                             mReadyState(nsIWebSocket::CONNECTING),
                             mOutgoingBufferedAmount(0),
                             mBinaryType(WS_BINARY_TYPE_BLOB),
                             mScriptLine(0),
                             mInnerWindowID(0)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");
  nsLayoutStatics::AddRef();
}

nsWebSocket::~nsWebSocket()
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");

  Disconnect();
  nsLayoutStatics::Release();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsWebSocket)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsWebSocket,
                                                  nsDOMEventTargetWrapperCache)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnOpenListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnMessageListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnCloseListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnErrorListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mPrincipal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mURI)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mChannel)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsWebSocket,
                                                nsDOMEventTargetWrapperCache)
  tmp->Disconnect();
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnOpenListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnMessageListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnCloseListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnErrorListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mPrincipal)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mURI)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mChannel)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

DOMCI_DATA(WebSocket, nsWebSocket)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsWebSocket)
  NS_INTERFACE_MAP_ENTRY(nsIWebSocket)
  NS_INTERFACE_MAP_ENTRY(nsIJSNativeInitializer)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY(nsIWebSocketListener)
  NS_INTERFACE_MAP_ENTRY(nsIRequest)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(WebSocket)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetWrapperCache)

NS_IMPL_ADDREF_INHERITED(nsWebSocket, nsDOMEventTargetWrapperCache)
NS_IMPL_RELEASE_INHERITED(nsWebSocket, nsDOMEventTargetWrapperCache)

//-----------------------------------------------------------------------------
// nsWebSocket::nsIJSNativeInitializer methods:
//-----------------------------------------------------------------------------

/**
 * This Initialize method is called from XPConnect via nsIJSNativeInitializer.
 * It is used for constructing our nsWebSocket from JavaScript. It expects a URL
 * string parameter and an optional protocol parameter which may be a string or
 * an array of strings. It also initializes the principal, the script context and
 * the window owner.
 */
NS_IMETHODIMP
nsWebSocket::Initialize(nsISupports* aOwner,
                        JSContext* aContext,
                        JSObject* aObject,
                        PRUint32 aArgc,
                        jsval* aArgv)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");
  nsAutoString urlParam;

  if (!PrefEnabled()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  if (aArgc != 1 && aArgc != 2) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  JSAutoRequest ar(aContext);

  JSString* jsstr = JS_ValueToString(aContext, aArgv[0]);
  if (!jsstr) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  JS::Anchor<JSString *> deleteProtector(jsstr);
  size_t length;
  const jschar *chars = JS_GetStringCharsAndLength(aContext, jsstr, &length);
  if (!chars) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  urlParam.Assign(chars, length);
  deleteProtector.clear();

  nsCOMPtr<nsPIDOMWindow> ownerWindow = do_QueryInterface(aOwner);
  NS_ENSURE_STATE(ownerWindow);

  nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface(aOwner);
  NS_ENSURE_STATE(sgo);
  nsCOMPtr<nsIScriptContext> scriptContext = sgo->GetContext();
  NS_ENSURE_STATE(scriptContext);

  nsCOMPtr<nsIScriptObjectPrincipal> scriptPrincipal(do_QueryInterface(aOwner));
  NS_ENSURE_STATE(scriptPrincipal);
  nsCOMPtr<nsIPrincipal> principal = scriptPrincipal->GetPrincipal();
  NS_ENSURE_STATE(principal);

  nsTArray<nsString> protocolArray;

  if (aArgc == 2) {
    JSObject *jsobj;

    if (JSVAL_IS_OBJECT(aArgv[1]) &&
        (jsobj = JSVAL_TO_OBJECT(aArgv[1])) &&
        JS_IsArrayObject(aContext, jsobj)) {
      jsuint len;
      JS_GetArrayLength(aContext, jsobj, &len);
      
      for (PRUint32 index = 0; index < len; ++index) {
        jsval value;

        if (!JS_GetElement(aContext, jsobj, index, &value))
          return NS_ERROR_DOM_SYNTAX_ERR;

        jsstr = JS_ValueToString(aContext, value);
        if (!jsstr)
          return NS_ERROR_DOM_SYNTAX_ERR;

        deleteProtector.set(jsstr);
        chars = JS_GetStringCharsAndLength(aContext, jsstr, &length);
        if (!chars)
          return NS_ERROR_OUT_OF_MEMORY;

        nsDependentString protocolElement(chars, length);
        if (protocolElement.IsEmpty())
          return NS_ERROR_DOM_SYNTAX_ERR;
        if (protocolArray.Contains(protocolElement))
          return NS_ERROR_DOM_SYNTAX_ERR;
        if (protocolElement.FindChar(',') != -1)  /* interferes w/list */
          return NS_ERROR_DOM_SYNTAX_ERR;
        protocolArray.AppendElement(protocolElement);
        deleteProtector.clear();
      }
    } else {
      jsstr = JS_ValueToString(aContext, aArgv[1]);
      if (!jsstr)
        return NS_ERROR_DOM_SYNTAX_ERR;
      
      deleteProtector.set(jsstr);
      chars = JS_GetStringCharsAndLength(aContext, jsstr, &length);
      if (!chars)
        return NS_ERROR_OUT_OF_MEMORY;
      
      nsDependentString protocolElement(chars, length);
      if (protocolElement.IsEmpty())
        return NS_ERROR_DOM_SYNTAX_ERR;
      if (protocolElement.FindChar(',') != -1)  /* interferes w/list */
        return NS_ERROR_DOM_SYNTAX_ERR;
      protocolArray.AppendElement(protocolElement);
    }
  }

  return Init(principal, scriptContext, ownerWindow, urlParam, protocolArray);
}

//-----------------------------------------------------------------------------
// nsWebSocket methods:
//-----------------------------------------------------------------------------

class nsAutoCloseWS
{
public:
  nsAutoCloseWS(nsWebSocket *aWebSocket)
    : mWebSocket(aWebSocket)
  {}

  ~nsAutoCloseWS()
  {
    if (!mWebSocket->mChannel) {
      mWebSocket->CloseConnection();
    }
  }
private:
  nsRefPtr<nsWebSocket> mWebSocket;
};

nsresult
nsWebSocket::EstablishConnection()
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");
  NS_ABORT_IF_FALSE(!mChannel, "mChannel should be null");

  nsresult rv;

  nsCOMPtr<nsIWebSocketChannel> wsChannel;
  nsAutoCloseWS autoClose(this);

  if (mSecure) {
    wsChannel =
      do_CreateInstance("@mozilla.org/network/protocol;1?name=wss", &rv);
  } else {
    wsChannel =
      do_CreateInstance("@mozilla.org/network/protocol;1?name=ws", &rv);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  rv = wsChannel->SetNotificationCallbacks(this);
  NS_ENSURE_SUCCESS(rv, rv);

  // add ourselves to the document's load group and
  // provide the http stack the loadgroup info too
  nsCOMPtr<nsILoadGroup> loadGroup;
  rv = GetLoadGroup(getter_AddRefs(loadGroup));
  if (loadGroup) {
    rv = wsChannel->SetLoadGroup(loadGroup);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = loadGroup->AddRequest(this, nsnull);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (!mRequestedProtocolList.IsEmpty()) {
    rv = wsChannel->SetProtocol(mRequestedProtocolList);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCString asciiOrigin;
  rv = nsContentUtils::GetASCIIOrigin(mPrincipal, asciiOrigin);
  NS_ENSURE_SUCCESS(rv, rv);

  ToLowerCase(asciiOrigin);

  rv = wsChannel->AsyncOpen(mURI, asciiOrigin, this, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  mChannel = wsChannel;

  return NS_OK;
}

class nsWSCloseEvent : public nsRunnable
{
public:
nsWSCloseEvent(nsWebSocket *aWebSocket, bool aWasClean, 
               PRUint16 aCode, const nsString &aReason)
    : mWebSocket(aWebSocket),
      mWasClean(aWasClean),
      mCode(aCode),
      mReason(aReason)
  {}

  NS_IMETHOD Run()
  {
    nsresult rv = mWebSocket->CreateAndDispatchCloseEvent(mWasClean,
                                                          mCode, mReason);
    mWebSocket->UpdateMustKeepAlive();
    return rv;
  }

private:
  nsRefPtr<nsWebSocket> mWebSocket;
  bool mWasClean;
  PRUint16 mCode;
  nsString mReason;
};

nsresult
nsWebSocket::CreateAndDispatchSimpleEvent(const nsString& aName)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");
  nsresult rv;

  rv = CheckInnerWindowCorrectness();
  if (NS_FAILED(rv)) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOMEvent> event;
  rv = NS_NewDOMEvent(getter_AddRefs(event), nsnull, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  // it doesn't bubble, and it isn't cancelable
  rv = event->InitEvent(aName, false, false);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPrivateDOMEvent> privateEvent = do_QueryInterface(event);
  rv = privateEvent->SetTrusted(true);
  NS_ENSURE_SUCCESS(rv, rv);

  return DispatchDOMEvent(nsnull, event, nsnull, nsnull);
}

nsresult
nsWebSocket::CreateAndDispatchMessageEvent(const nsACString& aData,
                                           bool isBinary)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");
  nsresult rv;

  rv = CheckInnerWindowCorrectness();
  if (NS_FAILED(rv))
    return NS_OK;

  // Get the JSContext
  nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface(mOwner);
  NS_ENSURE_TRUE(sgo, NS_ERROR_FAILURE);

  nsIScriptContext* scriptContext = sgo->GetContext();
  NS_ENSURE_TRUE(scriptContext, NS_ERROR_FAILURE);

  JSContext* cx = scriptContext->GetNativeContext();
  NS_ENSURE_TRUE(cx, NS_ERROR_FAILURE);

  // Create appropriate JS object for message
  jsval jsData;
  {
    JSAutoRequest ar(cx);
    if (isBinary) {
      if (mBinaryType == WS_BINARY_TYPE_BLOB) {
        rv = CreateResponseBlob(aData, cx, jsData);
        NS_ENSURE_SUCCESS(rv, rv);
      } else if (mBinaryType == WS_BINARY_TYPE_ARRAYBUFFER) {
        JSObject *arrayBuf;
        rv = nsContentUtils::CreateArrayBuffer(cx, aData, &arrayBuf);
        NS_ENSURE_SUCCESS(rv, rv);
        jsData = OBJECT_TO_JSVAL(arrayBuf);
      } else {
        NS_RUNTIMEABORT("Unknown binary type!");
        return NS_ERROR_UNEXPECTED;
      }
    } else {
      // JS string
      NS_ConvertUTF8toUTF16 utf16Data(aData);
      JSString* jsString;
      jsString = JS_NewUCStringCopyN(cx, utf16Data.get(), utf16Data.Length());
      NS_ENSURE_TRUE(jsString, NS_ERROR_FAILURE);

      jsData = STRING_TO_JSVAL(jsString);
    }
  }

  // create an event that uses the MessageEvent interface,
  // which does not bubble, is not cancelable, and has no default action

  nsCOMPtr<nsIDOMEvent> event;
  rv = NS_NewDOMMessageEvent(getter_AddRefs(event), nsnull, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMMessageEvent> messageEvent = do_QueryInterface(event);
  rv = messageEvent->InitMessageEvent(NS_LITERAL_STRING("message"),
                                      false, false,
                                      jsData,
                                      mUTF16Origin,
                                      EmptyString(), nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPrivateDOMEvent> privateEvent = do_QueryInterface(event);
  rv = privateEvent->SetTrusted(true);
  NS_ENSURE_SUCCESS(rv, rv);

  return DispatchDOMEvent(nsnull, event, nsnull, nsnull);
}

// Initial implementation: only stores to RAM, not file
// TODO: bug 704447: large file support
nsresult
nsWebSocket::CreateResponseBlob(const nsACString& aData, JSContext *aCx,
                                jsval &jsData)
{
  PRUint32 blobLen = aData.Length();
  void *blobData = PR_Malloc(blobLen);
  nsCOMPtr<nsIDOMBlob> blob;
  if (blobData) {
    memcpy(blobData, aData.BeginReading(), blobLen);
    blob = new nsDOMMemoryFile(blobData, blobLen, EmptyString());
  } else {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  JSObject* scope = JS_GetGlobalForScopeChain(aCx);
  return nsContentUtils::WrapNative(aCx, scope, blob, &jsData, nsnull, true);
}

nsresult
nsWebSocket::CreateAndDispatchCloseEvent(bool aWasClean,
                                         PRUint16 aCode,
                                         const nsString &aReason)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");
  nsresult rv;

  mTriggeredCloseEvent = true;

  rv = CheckInnerWindowCorrectness();
  if (NS_FAILED(rv)) {
    return NS_OK;
  }

  // create an event that uses the CloseEvent interface,
  // which does not bubble, is not cancelable, and has no default action

  nsCOMPtr<nsIDOMEvent> event;
  rv = NS_NewDOMCloseEvent(getter_AddRefs(event), nsnull, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMCloseEvent> closeEvent = do_QueryInterface(event);
  rv = closeEvent->InitCloseEvent(NS_LITERAL_STRING("close"),
                                  false, false,
                                  aWasClean, aCode, aReason);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPrivateDOMEvent> privateEvent = do_QueryInterface(event);
  rv = privateEvent->SetTrusted(true);
  NS_ENSURE_SUCCESS(rv, rv);

  return DispatchDOMEvent(nsnull, event, nsnull, nsnull);
}

bool
nsWebSocket::PrefEnabled()
{
  return Preferences::GetBool("network.websocket.enabled", true);
}

void
nsWebSocket::SetReadyState(PRUint16 aNewReadyState)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");
  nsresult rv;

  if (mReadyState == aNewReadyState) {
    return;
  }

  NS_ABORT_IF_FALSE((aNewReadyState == nsIWebSocket::OPEN)    ||
                    (aNewReadyState == nsIWebSocket::CLOSING) ||
                    (aNewReadyState == nsIWebSocket::CLOSED),
                    "unexpected readyState");

  if (aNewReadyState == nsIWebSocket::OPEN) {
    NS_ABORT_IF_FALSE(mReadyState == nsIWebSocket::CONNECTING,
                      "unexpected readyState transition");
    mReadyState = aNewReadyState;

    rv = CreateAndDispatchSimpleEvent(NS_LITERAL_STRING("open"));
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to dispatch the open event");
    }
    UpdateMustKeepAlive();
    return;
  }

  if (aNewReadyState == nsIWebSocket::CLOSING) {
    NS_ABORT_IF_FALSE((mReadyState == nsIWebSocket::CONNECTING) ||
                      (mReadyState == nsIWebSocket::OPEN),
                      "unexpected readyState transition");
    mReadyState = aNewReadyState;
    return;
  }

  if (aNewReadyState == nsIWebSocket::CLOSED) {
    mReadyState = aNewReadyState;

    // The close event must be dispatched asynchronously.
    rv = NS_DispatchToMainThread(new nsWSCloseEvent(this, mClosedCleanly,
                                                    mServerReasonCode,
                                                    mServerReason),
                                 NS_DISPATCH_NORMAL);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to dispatch the close event");
      mTriggeredCloseEvent = true;
      UpdateMustKeepAlive();
    }
  }
}

nsresult
nsWebSocket::ParseURL(const nsString& aURL)
{
  nsresult rv;

  NS_ENSURE_TRUE(!aURL.IsEmpty(), NS_ERROR_DOM_SYNTAX_ERR);

  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri), aURL);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_SYNTAX_ERR);

  nsCOMPtr<nsIURL> parsedURL(do_QueryInterface(uri, &rv));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_SYNTAX_ERR);

  nsCAutoString fragment;
  rv = parsedURL->GetRef(fragment);
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && fragment.IsEmpty(),
                 NS_ERROR_DOM_SYNTAX_ERR);

  nsCAutoString scheme;
  rv = parsedURL->GetScheme(scheme);
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && !scheme.IsEmpty(),
                 NS_ERROR_DOM_SYNTAX_ERR);

  nsCAutoString host;
  rv = parsedURL->GetAsciiHost(host);
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && !host.IsEmpty(), NS_ERROR_DOM_SYNTAX_ERR);

  PRInt32 port;
  rv = parsedURL->GetPort(&port);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_SYNTAX_ERR);

  rv = NS_CheckPortSafety(port, scheme.get());
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_SYNTAX_ERR);

  nsCAutoString filePath;
  rv = parsedURL->GetFilePath(filePath);
  if (filePath.IsEmpty()) {
    filePath.AssignLiteral("/");
  }
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_SYNTAX_ERR);

  nsCAutoString query;
  rv = parsedURL->GetQuery(query);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_SYNTAX_ERR);

  if (scheme.LowerCaseEqualsLiteral("ws")) {
     mSecure = false;
     mPort = (port == -1) ? DEFAULT_WS_SCHEME_PORT : port;
  } else if (scheme.LowerCaseEqualsLiteral("wss")) {
    mSecure = true;
    mPort = (port == -1) ? DEFAULT_WSS_SCHEME_PORT : port;
  } else {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  rv = nsContentUtils::GetUTFOrigin(parsedURL, mUTF16Origin);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_SYNTAX_ERR);

  mAsciiHost = host;
  ToLowerCase(mAsciiHost);

  mResource = filePath;
  if (!query.IsEmpty()) {
    mResource.AppendLiteral("?");
    mResource.Append(query);
  }
  PRUint32 length = mResource.Length();
  PRUint32 i;
  for (i = 0; i < length; ++i) {
    if (mResource[i] < static_cast<PRUnichar>(0x0021) ||
        mResource[i] > static_cast<PRUnichar>(0x007E)) {
      return NS_ERROR_DOM_SYNTAX_ERR;
    }
  }

  mOriginalURL = aURL;
  mURI = parsedURL;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// Methods that keep alive the WebSocket object when:
//   1. the object has registered event listeners that can be triggered
//      ("strong event listeners");
//   2. there are outgoing not sent messages.
//-----------------------------------------------------------------------------

void
nsWebSocket::UpdateMustKeepAlive()
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");
  if (!mCheckMustKeepAlive) {
    return;
  }

  bool shouldKeepAlive = false;

  if (mListenerManager) {
    switch (mReadyState)
    {
      case nsIWebSocket::CONNECTING:
      {
        if (mListenerManager->HasListenersFor(NS_LITERAL_STRING("open")) ||
            mListenerManager->HasListenersFor(NS_LITERAL_STRING("message")) ||
            mListenerManager->HasListenersFor(NS_LITERAL_STRING("error")) ||
            mListenerManager->HasListenersFor(NS_LITERAL_STRING("close"))) {
          shouldKeepAlive = true;
        }
      }
      break;

      case nsIWebSocket::OPEN:
      case nsIWebSocket::CLOSING:
      {
        if (mListenerManager->HasListenersFor(NS_LITERAL_STRING("message")) ||
            mListenerManager->HasListenersFor(NS_LITERAL_STRING("error")) ||
            mListenerManager->HasListenersFor(NS_LITERAL_STRING("close")) ||
            mOutgoingBufferedAmount != 0) {
          shouldKeepAlive = true;
        }
      }
      break;

      case nsIWebSocket::CLOSED:
      {
        shouldKeepAlive =
          (!mTriggeredCloseEvent &&
           mListenerManager->HasListenersFor(NS_LITERAL_STRING("close")));
      }
    }
  }

  if (mKeepingAlive && !shouldKeepAlive) {
    mKeepingAlive = false;
    static_cast<nsIDOMEventTarget*>(this)->Release();
  } else if (!mKeepingAlive && shouldKeepAlive) {
    mKeepingAlive = true;
    static_cast<nsIDOMEventTarget*>(this)->AddRef();
  }
}

void
nsWebSocket::DontKeepAliveAnyMore()
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");
  if (mKeepingAlive) {
    mKeepingAlive = false;
    static_cast<nsIDOMEventTarget*>(this)->Release();
  }
  mCheckMustKeepAlive = false;
}

nsresult
nsWebSocket::UpdateURI()
{
  // Check for Redirections
  nsCOMPtr<nsIURI> uri;
  nsresult rv = mChannel->GetURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString spec;
  rv = uri->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);
  CopyUTF8toUTF16(spec, mEffectiveURL);

  bool isWSS = false;
  rv = uri->SchemeIs("wss", &isWSS);
  NS_ENSURE_SUCCESS(rv, rv);
  mSecure = isWSS ? true : false;

  return NS_OK;
}

NS_IMETHODIMP
nsWebSocket::RemoveEventListener(const nsAString& aType,
                                 nsIDOMEventListener* aListener,
                                 bool aUseCapture)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");
  nsresult rv = nsDOMEventTargetHelper::RemoveEventListener(aType,
                                                            aListener,
                                                            aUseCapture);
  if (NS_SUCCEEDED(rv)) {
    UpdateMustKeepAlive();
  }
  return rv;
}

NS_IMETHODIMP
nsWebSocket::AddEventListener(const nsAString& aType,
                              nsIDOMEventListener *aListener,
                              bool aUseCapture,
                              bool aWantsUntrusted,
                              PRUint8 optional_argc)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");
  nsresult rv = nsDOMEventTargetHelper::AddEventListener(aType,
                                                         aListener,
                                                         aUseCapture,
                                                         aWantsUntrusted,
                                                         optional_argc);
  if (NS_SUCCEEDED(rv)) {
    UpdateMustKeepAlive();
  }
  return rv;
}

//-----------------------------------------------------------------------------
// nsWebSocket::nsIWebSocket methods:
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsWebSocket::GetUrl(nsAString& aURL)
{
  if (mEffectiveURL.IsEmpty()) {
    aURL = mOriginalURL;
  } else {
    aURL = mEffectiveURL;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWebSocket::GetExtensions(nsAString& aExtensions)
{
  CopyUTF8toUTF16(mEstablishedExtensions, aExtensions);
  return NS_OK;
}

NS_IMETHODIMP
nsWebSocket::GetProtocol(nsAString& aProtocol)
{
  CopyUTF8toUTF16(mEstablishedProtocol, aProtocol);
  return NS_OK;
}

NS_IMETHODIMP
nsWebSocket::GetReadyState(PRUint16 *aReadyState)
{
  *aReadyState = mReadyState;
  return NS_OK;
}

NS_IMETHODIMP
nsWebSocket::GetBufferedAmount(PRUint32 *aBufferedAmount)
{
  *aBufferedAmount = mOutgoingBufferedAmount;
  return NS_OK;
}

NS_IMETHODIMP
nsWebSocket::GetBinaryType(nsAString& aBinaryType)
{
  switch (mBinaryType) {
  case WS_BINARY_TYPE_ARRAYBUFFER:
    aBinaryType.AssignLiteral("arraybuffer");
    break;
  case WS_BINARY_TYPE_BLOB:
    aBinaryType.AssignLiteral("blob");
    break;
  default:
    NS_ERROR("Should not happen");
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWebSocket::SetBinaryType(const nsAString& aBinaryType)
{
  if (aBinaryType.EqualsLiteral("arraybuffer")) {
    mBinaryType = WS_BINARY_TYPE_ARRAYBUFFER;
  } else if (aBinaryType.EqualsLiteral("blob")) {
    mBinaryType = WS_BINARY_TYPE_BLOB;
  } else  {
    return NS_ERROR_INVALID_ARG;
  }

  return NS_OK;
}

#define NS_WEBSOCKET_IMPL_DOMEVENTLISTENER(_eventlistenername, _eventlistener) \
  NS_IMETHODIMP                                                                \
  nsWebSocket::GetOn##_eventlistenername(nsIDOMEventListener * *aEventListener)\
  {                                                                            \
    return GetInnerEventListener(_eventlistener, aEventListener);              \
  }                                                                            \
                                                                               \
  NS_IMETHODIMP                                                                \
  nsWebSocket::SetOn##_eventlistenername(nsIDOMEventListener * aEventListener) \
  {                                                                            \
    return RemoveAddEventListener(NS_LITERAL_STRING(#_eventlistenername),      \
                                  _eventlistener, aEventListener);             \
  }

NS_WEBSOCKET_IMPL_DOMEVENTLISTENER(open, mOnOpenListener)
NS_WEBSOCKET_IMPL_DOMEVENTLISTENER(error, mOnErrorListener)
NS_WEBSOCKET_IMPL_DOMEVENTLISTENER(message, mOnMessageListener)
NS_WEBSOCKET_IMPL_DOMEVENTLISTENER(close, mOnCloseListener)

static bool
ContainsUnpairedSurrogates(const nsAString& aData)
{
  // Check for unpaired surrogates.
  PRUint32 i, length = aData.Length();
  for (i = 0; i < length; ++i) {
    if (NS_IS_LOW_SURROGATE(aData[i])) {
      return true;
    }
    if (NS_IS_HIGH_SURROGATE(aData[i])) {
      ++i;
      if (i == length || !NS_IS_LOW_SURROGATE(aData[i])) {
        return true;
      }
      continue;
    }
  }
  return false;
}

NS_IMETHODIMP
nsWebSocket::Send(nsIVariant *aData)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");

  if (mReadyState == nsIWebSocket::CONNECTING) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  nsCString msgString;
  nsCOMPtr<nsIInputStream> msgStream;
  bool isBinary;
  PRUint32 msgLen;
  nsresult rv = GetSendParams(aData, msgString, msgStream, isBinary, msgLen);
  NS_ENSURE_SUCCESS(rv, rv);

  // Always increment outgoing buffer len, even if closed
  mOutgoingBufferedAmount += msgLen;

  if (mReadyState == nsIWebSocket::CLOSING ||
      mReadyState == nsIWebSocket::CLOSED) {
    return NS_OK;
  }

  NS_ASSERTION(mReadyState == nsIWebSocket::OPEN,
               "Unknown state in nsWebSocket::Send");

  if (msgStream) {
    rv = mChannel->SendBinaryStream(msgStream, msgLen);
  } else {
    if (isBinary) {
      rv = mChannel->SendBinaryMsg(msgString);
    } else {
      rv = mChannel->SendMsg(msgString);
    }
  }
  NS_ENSURE_SUCCESS(rv, rv);

  UpdateMustKeepAlive();

  return NS_OK;
}

nsresult
nsWebSocket::GetSendParams(nsIVariant *aData, nsCString &aStringOut,
                           nsCOMPtr<nsIInputStream> &aStreamOut,
                           bool &aIsBinary, PRUint32 &aOutgoingLength)
{
  // Get type of data (arraybuffer, blob, or string)
  PRUint16 dataType;
  nsresult rv = aData->GetDataType(&dataType);
  NS_ENSURE_SUCCESS(rv, rv);

  if (dataType == nsIDataType::VTYPE_INTERFACE ||
      dataType == nsIDataType::VTYPE_INTERFACE_IS) {
    nsCOMPtr<nsISupports> supports;
    nsID *iid;
    rv = aData->GetAsInterface(&iid, getter_AddRefs(supports));
    NS_ENSURE_SUCCESS(rv, rv);

    nsMemory::Free(iid);

    // ArrayBuffer?
    jsval realVal;
    JSObject* obj;
    nsresult rv = aData->GetAsJSVal(&realVal);
    if (NS_SUCCEEDED(rv) && !JSVAL_IS_PRIMITIVE(realVal) &&
        (obj = JSVAL_TO_OBJECT(realVal)) &&
        (js_IsArrayBuffer(obj))) {
      PRInt32 len = JS_GetArrayBufferByteLength(obj);
      char* data = (char*)JS_GetArrayBufferData(obj);

      aStringOut.Assign(data, len);
      aIsBinary = true;
      aOutgoingLength = len;
      return NS_OK;
    }

    // Blob?
    nsCOMPtr<nsIDOMBlob> blob = do_QueryInterface(supports);
    if (blob) {
      rv = blob->GetInternalStream(getter_AddRefs(aStreamOut));
      NS_ENSURE_SUCCESS(rv, rv);

      // GetSize() should not perform blocking I/O (unlike Available())
      PRUint64 blobLen;
      rv = blob->GetSize(&blobLen);
      NS_ENSURE_SUCCESS(rv, rv);
      if (blobLen > PR_UINT32_MAX) {
        return NS_ERROR_FILE_TOO_BIG;
      }
      aOutgoingLength = static_cast<PRUint32>(blobLen);

      aIsBinary = true;
      return NS_OK;
    }
  }

  // Text message: if not already a string, turn it into one.
  // TODO: bug 704444: Correctly coerce any JS type to string
  //
  PRUnichar* data = nsnull;
  PRUint32 len = 0;
  rv = aData->GetAsWStringWithSize(&len, &data);
  NS_ENSURE_SUCCESS(rv, rv);

  nsString text;
  text.Adopt(data, len);

  if (ContainsUnpairedSurrogates(text)) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  rv = ConvertTextToUTF8(text, aStringOut);
  NS_ENSURE_SUCCESS(rv, rv);

  aIsBinary = false;
  aOutgoingLength = aStringOut.Length();
  return NS_OK;
}

nsresult
nsWebSocket::ConvertTextToUTF8(const nsString& aMessage, nsCString& buf)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");
  nsresult rv;

  nsCOMPtr<nsICharsetConverterManager> ccm =
    do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);
  SUCCESS_OR_FAIL_WEBSOCKET(rv, rv);

  nsCOMPtr<nsIUnicodeEncoder> converter;
  rv = ccm->GetUnicodeEncoder("UTF-8", getter_AddRefs(converter));
  SUCCESS_OR_FAIL_WEBSOCKET(rv, rv);

  rv = converter->SetOutputErrorBehavior(nsIUnicodeEncoder::kOnError_Replace,
                                         nsnull, UTF_8_REPLACEMENT_CHAR);
  SUCCESS_OR_FAIL_WEBSOCKET(rv, rv);

  PRInt32 inLen = aMessage.Length();
  PRInt32 maxLen;
  rv = converter->GetMaxLength(aMessage.BeginReading(), inLen, &maxLen);
  SUCCESS_OR_FAIL_WEBSOCKET(rv, rv);

  buf.SetLength(maxLen);
  TRUE_OR_FAIL_WEBSOCKET(buf.Length() == static_cast<PRUint32>(maxLen),
                         NS_ERROR_OUT_OF_MEMORY);

  char* start = buf.BeginWriting();

  PRInt32 outLen = maxLen;
  rv = converter->Convert(aMessage.BeginReading(), &inLen, start, &outLen);
  if (NS_SUCCEEDED(rv)) {
    PRInt32 outLen2 = maxLen - outLen;
    rv = converter->Finish(start + outLen, &outLen2);
    outLen += outLen2;
  }
  if (NS_FAILED(rv) || rv == NS_ERROR_UENC_NOMAPPING) {
    // Yes, NS_ERROR_UENC_NOMAPPING is a success code
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  buf.SetLength(outLen);
  TRUE_OR_FAIL_WEBSOCKET(buf.Length() == static_cast<PRUint32>(outLen),
                         NS_ERROR_UNEXPECTED);

  return NS_OK;
}

NS_IMETHODIMP
nsWebSocket::Close(PRUint16 code, const nsAString & reason, PRUint8 argc)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");

  // the reason code is optional, but if provided it must be in a specific range
  if (argc >= 1) {
    if (code != 1000 && (code < 3000 || code > 4999))
      return NS_ERROR_DOM_INVALID_ACCESS_ERR;
  }

  nsCAutoString utf8Reason;
  if (argc >= 2) {
    if (ContainsUnpairedSurrogates(reason))
      return NS_ERROR_DOM_SYNTAX_ERR;

    CopyUTF16toUTF8(reason, utf8Reason);

    // The API requires the UTF-8 string to be 123 or less bytes
    if (utf8Reason.Length() > 123)
      return NS_ERROR_DOM_SYNTAX_ERR;
  }

  // Format checks for reason and code both passed, they can now be assigned.
  if (argc >= 1)
    mClientReasonCode = code;
  if (argc >= 2)
    mClientReason = utf8Reason;
  
  if (mReadyState == nsIWebSocket::CLOSING ||
      mReadyState == nsIWebSocket::CLOSED) {
    return NS_OK;
  }

  if (mReadyState == nsIWebSocket::CONNECTING) {
    // FailConnection() can release the object, so we keep a reference
    // before calling it
    nsRefPtr<nsWebSocket> kungfuDeathGrip = this;

    FailConnection();
    return NS_OK;
  }

  // mReadyState == nsIWebSocket::OPEN
  CloseConnection();

  return NS_OK;
}

/**
 * This Init method should only be called by C++ consumers.
 */
NS_IMETHODIMP
nsWebSocket::Init(nsIPrincipal* aPrincipal,
                  nsIScriptContext* aScriptContext,
                  nsPIDOMWindow* aOwnerWindow,
                  const nsAString& aURL,
                  nsTArray<nsString> & protocolArray)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");
  nsresult rv;

  NS_ENSURE_ARG(aPrincipal);

  if (!PrefEnabled()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  mPrincipal = aPrincipal;
  mScriptContext = aScriptContext;
  if (aOwnerWindow) {
    mOwner = aOwnerWindow->IsOuterWindow() ?
      aOwnerWindow->GetCurrentInnerWindow() : aOwnerWindow;
  } else {
    mOwner = nsnull;
  }

  nsCOMPtr<nsIJSContextStack> stack =
    do_GetService("@mozilla.org/js/xpc/ContextStack;1");
  JSContext* cx = nsnull;
  if (stack && NS_SUCCEEDED(stack->Peek(&cx)) && cx) {
    JSStackFrame *fp = JS_GetScriptedCaller(cx, NULL);
    if (fp) {
      JSScript *script = JS_GetFrameScript(cx, fp);
      if (script) {
        mScriptFile = JS_GetScriptFilename(cx, script);
      }

      jsbytecode *pc = JS_GetFramePC(cx, fp);
      if (script && pc) {
        mScriptLine = JS_PCToLineNumber(cx, script, pc);
      }
    }

    mInnerWindowID = nsJSUtils::GetCurrentlyRunningCodeInnerWindowID(cx);
  }

  // parses the url
  rv = ParseURL(PromiseFlatString(aURL));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDocument> originDoc =
    nsContentUtils::GetDocumentFromScriptContext(mScriptContext);

  // Don't allow https:// to open ws://
  if (!mSecure &&
      !Preferences::GetBool("network.websocket.allowInsecureFromHTTPS",
                            false)) {
    // Confirmed we are opening plain ws:// and want to prevent this from a
    // secure context (e.g. https). Check the security context of the document
    // associated with this script, which is the same as associated with mOwner.
    if (originDoc && originDoc->GetSecurityInfo())
      return NS_ERROR_DOM_SECURITY_ERR;
  }

  // Assign the sub protocol list and scan it for illegal values
  for (PRUint32 index = 0; index < protocolArray.Length(); ++index) {
    for (PRUint32 i = 0; i < protocolArray[index].Length(); ++i) {
      if (protocolArray[index][i] < static_cast<PRUnichar>(0x0021) ||
          protocolArray[index][i] > static_cast<PRUnichar>(0x007E))
        return NS_ERROR_DOM_SYNTAX_ERR;
    }

    if (!mRequestedProtocolList.IsEmpty())
      mRequestedProtocolList.Append(NS_LITERAL_CSTRING(", "));
    AppendUTF16toUTF8(protocolArray[index], mRequestedProtocolList);
  }

  // Check content policy.
  PRInt16 shouldLoad = nsIContentPolicy::ACCEPT;
  rv = NS_CheckContentLoadPolicy(nsIContentPolicy::TYPE_WEBSOCKET,
                                 mURI,
                                 mPrincipal,
                                 originDoc,
                                 EmptyCString(),
                                 nsnull,
                                 &shouldLoad,
                                 nsContentUtils::GetContentPolicy(),
                                 nsContentUtils::GetSecurityManager());
  NS_ENSURE_SUCCESS(rv, rv);
  if (NS_CP_REJECTED(shouldLoad)) {
    // Disallowed by content policy.
    return NS_ERROR_CONTENT_BLOCKED;
  }

  // the constructor should throw a SYNTAX_ERROR only if it fails to parse the
  // url parameter, so we don't care about the EstablishConnection result.
  EstablishConnection();

  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsWebSocket::nsIRequest
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsWebSocket::GetName(nsACString &aName)
{
  CopyUTF16toUTF8(mOriginalURL, aName);
  return NS_OK;
}

NS_IMETHODIMP
nsWebSocket::IsPending(bool *aValue)
{
  *aValue = !mDisconnected;
  return NS_OK;
}

NS_IMETHODIMP
nsWebSocket::GetStatus(nsresult *aStatus)
{
  *aStatus = NS_OK;
  return NS_OK;
}

// Window closed, stop/reload button pressed, user navigated away from page, etc.
NS_IMETHODIMP
nsWebSocket::Cancel(nsresult aStatus)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");

  if (mDisconnected)
    return NS_OK;

  ConsoleError();
  mClientReasonCode = nsIWebSocketChannel::CLOSE_GOING_AWAY;
  return CloseConnection();
}

NS_IMETHODIMP
nsWebSocket::Suspend()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWebSocket::Resume()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWebSocket::GetLoadGroup(nsILoadGroup **aLoadGroup)
{
  *aLoadGroup = nsnull;

  nsCOMPtr<nsIDocument> doc =
    nsContentUtils::GetDocumentFromScriptContext(mScriptContext);

  if (doc) {
    *aLoadGroup = doc->GetDocumentLoadGroup().get();  // already_AddRefed
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWebSocket::SetLoadGroup(nsILoadGroup *aLoadGroup)
{
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsWebSocket::GetLoadFlags(nsLoadFlags *aLoadFlags)
{
  *aLoadFlags = nsIRequest::LOAD_BACKGROUND;
  return NS_OK;
}

NS_IMETHODIMP
nsWebSocket::SetLoadFlags(nsLoadFlags aLoadFlags)
{
  // we won't change the load flags at all.
  return NS_OK;
}
