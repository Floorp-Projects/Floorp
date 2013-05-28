/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMDataChannel.h"

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


#include "nsDOMDataChannelDeclarations.h"
#include "nsIDOMFile.h"
#include "nsIDOMDataChannel.h"
#include "nsIDOMMessageEvent.h"
#include "nsDOMEventTargetHelper.h"

#include "nsError.h"
#include "nsAutoPtr.h"
#include "nsContentUtils.h"
#include "nsCxPusher.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsNetUtil.h"
#include "nsDOMFile.h"

#include "DataChannel.h"

using namespace mozilla;
using namespace mozilla::dom;

nsDOMDataChannel::~nsDOMDataChannel()
{
  // Don't call us anymore!  Likely isn't an issue (or maybe just less of
  // one) once we block GC until all the (appropriate) onXxxx handlers
  // are dropped. (See WebRTC spec)
  LOG(("Close()ing %p", mDataChannel.get()));
  mDataChannel->SetListener(nullptr, nullptr);
  mDataChannel->Close();
}

/* virtual */ JSObject*
nsDOMDataChannel::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return DataChannelBinding::Wrap(aCx, aScope, this);
}

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
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

nsresult
nsDOMDataChannel::Init(nsPIDOMWindow* aDOMWindow)
{
  nsresult rv;
  nsAutoString urlParam;

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

uint16_t
nsDOMDataChannel::Stream() const
{
  return mDataChannel->GetStream();
}

NS_IMETHODIMP
nsDOMDataChannel::GetStream(uint16_t *aStream)
{
  *aStream = Stream();
  return NS_OK;
}

// XXX should be GetType()?  Open question for the spec
bool
nsDOMDataChannel::Reliable() const
{
  return mDataChannel->GetType() == mozilla::DataChannelConnection::RELIABLE;
}

NS_IMETHODIMP
nsDOMDataChannel::GetReliable(bool* aReliable)
{
  *aReliable = Reliable();
  return NS_OK;
}

bool
nsDOMDataChannel::Ordered() const
{
  return mDataChannel->GetOrdered();
}

NS_IMETHODIMP
nsDOMDataChannel::GetOrdered(bool* aOrdered)
{
  *aOrdered = Ordered();
  return NS_OK;
}

RTCDataChannelState
nsDOMDataChannel::ReadyState() const
{
  return static_cast<RTCDataChannelState>(mDataChannel->GetReadyState());
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

uint32_t
nsDOMDataChannel::BufferedAmount() const
{
  return mDataChannel->GetBufferedAmount();
}

NS_IMETHODIMP
nsDOMDataChannel::GetBufferedAmount(uint32_t* aBufferedAmount)
{
  *aBufferedAmount = BufferedAmount();
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

// All of the following is copy/pasted from WebSocket.cpp.
void
nsDOMDataChannel::Send(const nsAString& aData, ErrorResult& aRv)
{
  NS_ConvertUTF16toUTF8 msgString(aData);
  Send(nullptr, msgString, msgString.Length(), false, aRv);
}

void
nsDOMDataChannel::Send(nsIDOMBlob* aData, ErrorResult& aRv)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");

  nsCOMPtr<nsIInputStream> msgStream;
  nsresult rv = aData->GetInternalStream(getter_AddRefs(msgStream));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  uint64_t msgLength;
  rv = aData->GetSize(&msgLength);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  if (msgLength > UINT32_MAX) {
    aRv.Throw(NS_ERROR_FILE_TOO_BIG);
    return;
  }

  Send(msgStream, EmptyCString(), msgLength, true, aRv);
}

void
nsDOMDataChannel::Send(ArrayBuffer& aData, ErrorResult& aRv)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");

  MOZ_ASSERT(sizeof(*aData.Data()) == 1);
  uint32_t len = aData.Length();
  char* data = reinterpret_cast<char*>(aData.Data());

  nsDependentCSubstring msgString(data, len);
  Send(nullptr, msgString, len, true, aRv);
}

void
nsDOMDataChannel::Send(ArrayBufferView& aData, ErrorResult& aRv)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");

  MOZ_ASSERT(sizeof(*aData.Data()) == 1);
  uint32_t len = aData.Length();
  char* data = reinterpret_cast<char*>(aData.Data());

  nsDependentCSubstring msgString(data, len);
  Send(nullptr, msgString, len, true, aRv);
}

void
nsDOMDataChannel::Send(nsIInputStream* aMsgStream,
                       const nsACString& aMsgString,
                       uint32_t aMsgLength,
                       bool aIsBinary,
                       ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  uint16_t state = mDataChannel->GetReadyState();

  // In reality, the DataChannel protocol allows this, but we want it to
  // look like WebSockets
  if (state == mozilla::DataChannel::CONNECTING) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  if (state == mozilla::DataChannel::CLOSING ||
      state == mozilla::DataChannel::CLOSED) {
    return;
  }

  MOZ_ASSERT(state == mozilla::DataChannel::OPEN,
             "Unknown state in nsDOMDataChannel::Send");

  int32_t sent;
  if (aMsgStream) {
    sent = mDataChannel->SendBinaryStream(aMsgStream, aMsgLength);
  } else {
    if (aIsBinary) {
      sent = mDataChannel->SendBinaryMsg(aMsgString);
    } else {
      sent = mDataChannel->SendMsg(aMsgString);
    }
  }
  if (sent < 0) {
    aRv.Throw(NS_ERROR_FAILURE);
  }
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

  JS::Rooted<JS::Value> jsData(cx);

  if (aBinary) {
    if (mBinaryType == DC_BINARY_TYPE_BLOB) {
      rv = nsContentUtils::CreateBlobBuffer(cx, aData, &jsData);
      NS_ENSURE_SUCCESS(rv, rv);
    } else if (mBinaryType == DC_BINARY_TYPE_ARRAYBUFFER) {
      JS::Rooted<JSObject*> arrayBuf(cx);
      rv = nsContentUtils::CreateArrayBuffer(cx, aData, arrayBuf.address());
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
