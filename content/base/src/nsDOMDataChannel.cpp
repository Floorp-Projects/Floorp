/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG
#endif

#include "base/basictypes.h"
#include "prlog.h"

#ifdef PR_LOGGING
extern PRLogModuleInfo* GetDataChannelLog();
#endif
#undef LOG
#define LOG(args) PR_LOG(GetDataChannelLog(), PR_LOG_DEBUG, args)


#include "nsDOMDataChannel.h"
#include "nsIDOMFile.h"
#include "nsIJSNativeInitializer.h"
#include "nsIDOMDataChannel.h"
#include "nsIDOMRTCPeerConnection.h"
#include "nsIDOMMessageEvent.h"
#include "nsDOMClassInfo.h"
#include "nsDOMEventTargetHelper.h"

#include "js/Value.h"

#include "nsError.h"
#include "nsAutoPtr.h"
#include "nsContentUtils.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsJSUtils.h"
#include "nsNetUtil.h"
#include "nsDOMFile.h"

#include "DataChannel.h"

#ifdef GetBinaryType
// Windows apparently has a #define for GetBinaryType...
#undef GetBinaryType
#endif

using namespace mozilla;

class nsDOMDataChannel : public nsDOMEventTargetHelper,
                         public nsIDOMDataChannel,
                         public mozilla::DataChannelListener
{
public:
  nsDOMDataChannel(already_AddRefed<mozilla::DataChannel> aDataChannel)
    : mDataChannel(aDataChannel)
    , mBinaryType(DC_BINARY_TYPE_BLOB)
  {}

  ~nsDOMDataChannel()
  {
    // Don't call us anymore!  Likely isn't an issue (or maybe just less of
    // one) once we block GC until all the (appropriate) onXxxx handlers
    // are dropped. (See WebRTC spec)
    mDataChannel->SetListener(nullptr, nullptr);
    mDataChannel->Close();
  }

  nsresult Init(nsPIDOMWindow* aDOMWindow);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMDATACHANNEL

  NS_FORWARD_NSIDOMEVENTTARGET(nsDOMEventTargetHelper::)

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsDOMDataChannel,
                                           nsDOMEventTargetHelper)

  nsresult
  DoOnMessageAvailable(const nsACString& aMessage, bool aBinary);

  virtual nsresult
  OnMessageAvailable(nsISupports* aContext, const nsACString& aMessage);

  virtual nsresult
  OnBinaryMessageAvailable(nsISupports* aContext, const nsACString& aMessage);

  virtual nsresult OnSimpleEvent(nsISupports* aContext, const nsAString& aName);

  virtual nsresult
  OnChannelConnected(nsISupports* aContext);

  virtual nsresult
  OnChannelClosed(nsISupports* aContext);

  virtual void
  AppReady();

private:
  // Get msg info out of JS variable being sent (string, arraybuffer, blob)
  nsresult GetSendParams(nsIVariant *aData, nsCString &aStringOut,
                         nsCOMPtr<nsIInputStream> &aStreamOut,
                         bool &aIsBinary, uint32_t &aOutgoingLength);

  // Owning reference
  nsRefPtr<mozilla::DataChannel> mDataChannel;
  nsString  mOrigin;
  enum
  {
    DC_BINARY_TYPE_ARRAYBUFFER,
    DC_BINARY_TYPE_BLOB,
  } mBinaryType;
};

DOMCI_DATA(DataChannel, nsDOMDataChannel)
// A bit of a hack for RTCPeerConnection, since it doesn't have a .cpp file of
// its own.  Note that it's not castable to anything in particular other than
// nsIDOMRTCPeerConnection, so we can just use nsIDOMRTCPeerConnection as the
// "class".
DOMCI_DATA(RTCPeerConnection, nsIDOMRTCPeerConnection)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsDOMDataChannel,
                                                  nsDOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsDOMDataChannel,
                                                nsDOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ADDREF_INHERITED(nsDOMDataChannel, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(nsDOMDataChannel, nsDOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsDOMDataChannel)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDataChannel)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(DataChannel)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

nsresult
nsDOMDataChannel::Init(nsPIDOMWindow* aDOMWindow)
{
  nsresult rv;
  nsAutoString urlParam;

  nsDOMEventTargetHelper::Init();

  MOZ_ASSERT(mDataChannel);
  mDataChannel->SetListener(this, nullptr);

  // Now grovel through the objects to get a usable origin for onMessage
  nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface(aDOMWindow);
  NS_ENSURE_STATE(sgo);
  nsCOMPtr<nsIScriptContext> scriptContext = sgo->GetContext();
  NS_ENSURE_STATE(scriptContext);

  nsCOMPtr<nsIScriptObjectPrincipal> scriptPrincipal(do_QueryInterface(aDOMWindow));
  NS_ENSURE_STATE(scriptPrincipal);
  nsCOMPtr<nsIPrincipal> principal = scriptPrincipal->GetPrincipal();
  NS_ENSURE_STATE(principal);

  if (aDOMWindow) {
    BindToOwner(aDOMWindow->IsOuterWindow() ?
                aDOMWindow->GetCurrentInnerWindow() : aDOMWindow);
  } else {
    BindToOwner(aDOMWindow);
  }

  // Attempt to kill "ghost" DataChannel (if one can happen): but usually too early for check to fail
  rv = CheckInnerWindowCorrectness();
  NS_ENSURE_SUCCESS(rv,rv);

  // See bug 696085
  // We don't need to observe for window destroyed or frozen; but PeerConnection needs
  // to not allow itself to be bfcached (and get destroyed on navigation).

  rv = nsContentUtils::GetUTFOrigin(principal,mOrigin);
  LOG(("%s: origin = %s\n",__FUNCTION__,NS_LossyConvertUTF16toASCII(mOrigin).get()));
  return rv;
}

NS_IMPL_EVENT_HANDLER(nsDOMDataChannel, open)
NS_IMPL_EVENT_HANDLER(nsDOMDataChannel, error)
NS_IMPL_EVENT_HANDLER(nsDOMDataChannel, close)
NS_IMPL_EVENT_HANDLER(nsDOMDataChannel, message)

NS_IMETHODIMP
nsDOMDataChannel::GetLabel(nsAString& aLabel)
{
  mDataChannel->GetLabel(aLabel);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDataChannel::GetProtocol(nsAString& aProtocol)
{
  mDataChannel->GetProtocol(aProtocol);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDataChannel::GetStream(uint16_t *aStream)
{
  mDataChannel->GetStream(aStream);
  return NS_OK;
}

// XXX should be GetType()?  Open question for the spec
NS_IMETHODIMP
nsDOMDataChannel::GetReliable(bool* aReliable)
{
  *aReliable = (mDataChannel->GetType() == mozilla::DataChannelConnection::RELIABLE);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDataChannel::GetOrdered(bool* aOrdered)
{
  *aOrdered = mDataChannel->GetOrdered();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDataChannel::GetReadyState(nsAString& aReadyState)
{
  uint16_t readyState = mDataChannel->GetReadyState();
  // From the WebRTC spec
  const char * stateName[] = {
    "connecting",
    "open",
    "closing",
    "closed"
  };
  MOZ_ASSERT(/*readyState >= mozilla::DataChannel::CONNECTING && */ // Always true due to datatypes
             readyState <= mozilla::DataChannel::CLOSED);
  aReadyState.AssignASCII(stateName[readyState]);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMDataChannel::GetBufferedAmount(uint32_t* aBufferedAmount)
{
  *aBufferedAmount = mDataChannel->GetBufferedAmount();
  return NS_OK;
}

NS_IMETHODIMP nsDOMDataChannel::GetBinaryType(nsAString & aBinaryType)
{
  switch (mBinaryType) {
  case DC_BINARY_TYPE_ARRAYBUFFER:
    aBinaryType.AssignLiteral("arraybuffer");
    break;
  case DC_BINARY_TYPE_BLOB:
    aBinaryType.AssignLiteral("blob");
    break;
  default:
    NS_ERROR("Should not happen");
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDataChannel::SetBinaryType(const nsAString& aBinaryType)
{
  if (aBinaryType.EqualsLiteral("arraybuffer")) {
    mBinaryType = DC_BINARY_TYPE_ARRAYBUFFER;
  } else if (aBinaryType.EqualsLiteral("blob")) {
    mBinaryType = DC_BINARY_TYPE_BLOB;
  } else  {
    return NS_ERROR_INVALID_ARG;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDataChannel::Close()
{
  mDataChannel->Close();
  return NS_OK;
}

// Almost a clone of nsWebSocketChannel::Send()
NS_IMETHODIMP
nsDOMDataChannel::Send(nsIVariant* aData)
{
  MOZ_ASSERT(NS_IsMainThread());
  uint16_t state = mDataChannel->GetReadyState();

  // In reality, the DataChannel protocol allows this, but we want it to
  // look like WebSockets
  if (state == mozilla::DataChannel::CONNECTING) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  nsCString msgString;
  nsCOMPtr<nsIInputStream> msgStream;
  bool isBinary;
  uint32_t msgLen;
  nsresult rv = GetSendParams(aData, msgString, msgStream, isBinary, msgLen);
  NS_ENSURE_SUCCESS(rv, rv);

  if (state == mozilla::DataChannel::CLOSING ||
      state == mozilla::DataChannel::CLOSED) {
    return NS_OK;
  }

  MOZ_ASSERT(state == mozilla::DataChannel::OPEN,
             "Unknown state in nsWebSocket::Send");

  int32_t sent;
  if (msgStream) {
    sent = mDataChannel->SendBinaryStream(msgStream, msgLen);
  } else {
    if (isBinary) {
      sent = mDataChannel->SendBinaryMsg(msgString);
    } else {
      sent = mDataChannel->SendMsg(msgString);
    }
  }
  return sent >= 0 ? NS_OK : NS_ERROR_FAILURE;
}

// XXX Exact clone of nsWebSocketChannel::GetSendParams() - find a way to share!
nsresult
nsDOMDataChannel::GetSendParams(nsIVariant* aData, nsCString& aStringOut,
                                nsCOMPtr<nsIInputStream>& aStreamOut,
                                bool& aIsBinary, uint32_t& aOutgoingLength)
{
  // Get type of data (arraybuffer, blob, or string)
  uint16_t dataType;
  nsresult rv = aData->GetDataType(&dataType);
  NS_ENSURE_SUCCESS(rv, rv);

  if (dataType == nsIDataType::VTYPE_INTERFACE ||
      dataType == nsIDataType::VTYPE_INTERFACE_IS) {
    nsCOMPtr<nsISupports> supports;
    nsID* iid;
    rv = aData->GetAsInterface(&iid, getter_AddRefs(supports));
    NS_ENSURE_SUCCESS(rv, rv);

    nsMemory::Free(iid);

    // ArrayBuffer?
    JS::Value realVal;
    JSObject* obj;
    nsresult rv = aData->GetAsJSVal(&realVal);
    if (NS_SUCCEEDED(rv) && !JSVAL_IS_PRIMITIVE(realVal) &&
        (obj = JSVAL_TO_OBJECT(realVal)) &&
        (JS_IsArrayBufferObject(obj))) {
      int32_t len = JS_GetArrayBufferByteLength(obj);
      char* data = reinterpret_cast<char*>(JS_GetArrayBufferData(obj));

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
      uint64_t blobLen;
      rv = blob->GetSize(&blobLen);
      NS_ENSURE_SUCCESS(rv, rv);
      if (blobLen > UINT32_MAX) {
        return NS_ERROR_FILE_TOO_BIG;
      }
      aOutgoingLength = static_cast<uint32_t>(blobLen);

      aIsBinary = true;
      return NS_OK;
    }
  }

  // Text message: if not already a string, turn it into one.
  // TODO: bug 704444: Correctly coerce any JS type to string
  //
  PRUnichar* data = nullptr;
  uint32_t len = 0;
  rv = aData->GetAsWStringWithSize(&len, &data);
  NS_ENSURE_SUCCESS(rv, rv);

  nsString text;
  text.Adopt(data, len);

  CopyUTF16toUTF8(text, aStringOut);

  aIsBinary = false;
  aOutgoingLength = aStringOut.Length();
  return NS_OK;
}

nsresult
nsDOMDataChannel::DoOnMessageAvailable(const nsACString& aData,
                                       bool aBinary)
{
  MOZ_ASSERT(NS_IsMainThread());

  LOG(("DoOnMessageAvailable%s\n",aBinary ? ((mBinaryType == DC_BINARY_TYPE_BLOB) ? " (blob)" : " (binary)") : ""));

  nsresult rv = CheckInnerWindowCorrectness();
  if (NS_FAILED(rv)) {
    return NS_OK;
  }
  nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface(GetOwner());
  NS_ENSURE_TRUE(sgo, NS_ERROR_FAILURE);

  nsIScriptContext* sc = sgo->GetContext();
  NS_ENSURE_TRUE(sc, NS_ERROR_FAILURE);

  AutoPushJSContext cx(sc->GetNativeContext());
  NS_ENSURE_TRUE(cx, NS_ERROR_FAILURE);

  JSAutoRequest ar(cx);
  JS::Value jsData;

  if (aBinary) {
    if (mBinaryType == DC_BINARY_TYPE_BLOB) {
      rv = nsContentUtils::CreateBlobBuffer(cx, aData, jsData);
      NS_ENSURE_SUCCESS(rv, rv);
    } else if (mBinaryType == DC_BINARY_TYPE_ARRAYBUFFER) {
      JSObject* arrayBuf;
      rv = nsContentUtils::CreateArrayBuffer(cx, aData, &arrayBuf);
      NS_ENSURE_SUCCESS(rv, rv);
      jsData = OBJECT_TO_JSVAL(arrayBuf);
    } else {
      NS_RUNTIMEABORT("Unknown binary type!");
      return NS_ERROR_UNEXPECTED;
    }
  } else {
    NS_ConvertUTF8toUTF16 utf16data(aData);
    JSString* jsString = JS_NewUCStringCopyN(cx, utf16data.get(), utf16data.Length());
    NS_ENSURE_TRUE(jsString, NS_ERROR_FAILURE);

    jsData = STRING_TO_JSVAL(jsString);
  }

  nsCOMPtr<nsIDOMEvent> event;
  rv = NS_NewDOMMessageEvent(getter_AddRefs(event), this, nullptr, nullptr);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr<nsIDOMMessageEvent> messageEvent = do_QueryInterface(event);
  rv = messageEvent->InitMessageEvent(NS_LITERAL_STRING("message"),
                                      false, false,
                                      jsData, mOrigin, EmptyString(),
                                      nullptr);
  NS_ENSURE_SUCCESS(rv,rv);
  event->SetTrusted(true);

  LOG(("%p(%p): %s - Dispatching\n",this,(void*)mDataChannel,__FUNCTION__));
  rv = DispatchDOMEvent(nullptr, event, nullptr, nullptr);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to dispatch the message event!!!");
  }
  return rv;
}

nsresult
nsDOMDataChannel::OnMessageAvailable(nsISupports* aContext,
                                     const nsACString& aMessage)
{
  MOZ_ASSERT(NS_IsMainThread());
  return DoOnMessageAvailable(aMessage, false);
}

nsresult
nsDOMDataChannel::OnBinaryMessageAvailable(nsISupports* aContext,
                                           const nsACString& aMessage)
{
  MOZ_ASSERT(NS_IsMainThread());
  return DoOnMessageAvailable(aMessage, true);
}

nsresult
nsDOMDataChannel::OnSimpleEvent(nsISupports* aContext, const nsAString& aName)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv = CheckInnerWindowCorrectness();
  if (NS_FAILED(rv)) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOMEvent> event;
  rv = NS_NewDOMEvent(getter_AddRefs(event), this, nullptr, nullptr);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = event->InitEvent(aName, false, false);
  NS_ENSURE_SUCCESS(rv,rv);

  event->SetTrusted(true);

  return DispatchDOMEvent(nullptr, event, nullptr, nullptr);
}

nsresult
nsDOMDataChannel::OnChannelConnected(nsISupports* aContext)
{
  LOG(("%p(%p): %s - Dispatching\n",this,(void*)mDataChannel,__FUNCTION__));

  return OnSimpleEvent(aContext, NS_LITERAL_STRING("open"));
}

nsresult
nsDOMDataChannel::OnChannelClosed(nsISupports* aContext)
{
  LOG(("%p(%p): %s - Dispatching\n",this,(void*)mDataChannel,__FUNCTION__));

  return OnSimpleEvent(aContext, NS_LITERAL_STRING("close"));
}

void
nsDOMDataChannel::AppReady()
{
  mDataChannel->AppReady();
}

/* static */
nsresult
NS_NewDOMDataChannel(already_AddRefed<mozilla::DataChannel> aDataChannel,
                     nsPIDOMWindow* aWindow,
                     nsIDOMDataChannel** aDomDataChannel)
{
  nsRefPtr<nsDOMDataChannel> domdc = new nsDOMDataChannel(aDataChannel);

  nsresult rv = domdc->Init(aWindow);
  NS_ENSURE_SUCCESS(rv,rv);

  return CallQueryInterface(domdc, aDomDataChannel);
}

/* static */
void
NS_DataChannelAppReady(nsIDOMDataChannel* aDomDataChannel)
{
  ((nsDOMDataChannel *)aDomDataChannel)->AppReady();
}
