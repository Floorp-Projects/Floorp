/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"
#include "GeneratedEvents.h"
#include "nsContentUtils.h"
#include "nsCxPusher.h"
#include "nsDOMClassInfo.h"
#include "nsIDOMBluetoothDeviceEvent.h"
#include "nsIDOMBluetoothStatusChangedEvent.h"
#include "nsTArrayHelpers.h"
#include "DOMRequest.h"
#include "nsThreadUtils.h"

#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/LazyIdleThread.h"
#include "mozilla/Util.h"

#include "BluetoothAdapter.h"
#include "BluetoothDevice.h"
#include "BluetoothReplyRunnable.h"
#include "BluetoothService.h"
#include "BluetoothUtils.h"
#include "MediaMetaData.h"
#include "MediaPlayStatus.h"

using namespace mozilla;

USING_BLUETOOTH_NAMESPACE

DOMCI_DATA(BluetoothAdapter, BluetoothAdapter)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(BluetoothAdapter,
                                               nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mJsUuids)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mJsDeviceAddresses)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(BluetoothAdapter, 
                                                  nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(BluetoothAdapter, 
                                                nsDOMEventTargetHelper)
  tmp->Unroot();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(BluetoothAdapter)
  NS_INTERFACE_MAP_ENTRY(nsIDOMBluetoothAdapter)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(BluetoothAdapter)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(BluetoothAdapter, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(BluetoothAdapter, nsDOMEventTargetHelper)

class GetDevicesTask : public BluetoothReplyRunnable
{
public:
  GetDevicesTask(BluetoothAdapter* aAdapterPtr,
                       nsIDOMDOMRequest* aReq) :
    BluetoothReplyRunnable(aReq),
    mAdapterPtr(aAdapterPtr)
  {
    MOZ_ASSERT(aReq && aAdapterPtr);
  }

  virtual bool ParseSuccessfulReply(JS::Value* aValue)
  {
    *aValue = JSVAL_VOID;

    const BluetoothValue& v = mReply->get_BluetoothReplySuccess().value();
    if (v.type() != BluetoothValue::TArrayOfBluetoothNamedValue) {
      NS_WARNING("Not a BluetoothNamedValue array!");
      SetError(NS_LITERAL_STRING("BluetoothReplyTypeError"));
      return false;
    }

    const InfallibleTArray<BluetoothNamedValue>& values =
      v.get_ArrayOfBluetoothNamedValue();

    nsTArray<nsRefPtr<BluetoothDevice> > devices;
    JSObject* JsDevices;
    for (uint32_t i = 0; i < values.Length(); i++) {
      const BluetoothValue properties = values[i].value();
      if (properties.type() != BluetoothValue::TArrayOfBluetoothNamedValue) {
        NS_WARNING("Not a BluetoothNamedValue array!");
        SetError(NS_LITERAL_STRING("BluetoothReplyTypeError"));
        return false;
      }
      nsRefPtr<BluetoothDevice> d =
        BluetoothDevice::Create(mAdapterPtr->GetOwner(),
                                mAdapterPtr->GetPath(),
                                properties);
      devices.AppendElement(d);
    }

    nsresult rv;
    nsIScriptContext* sc = mAdapterPtr->GetContextForEventHandlers(&rv);
    if (!sc) {
      NS_WARNING("Cannot create script context!");
      SetError(NS_LITERAL_STRING("BluetoothScriptContextError"));
      return false;
    }

    AutoPushJSContext cx(sc->GetNativeContext());
    rv = nsTArrayToJSArray(cx, devices, &JsDevices);
    if (!JsDevices) {
      NS_WARNING("Cannot create JS array!");
      SetError(NS_LITERAL_STRING("BluetoothError"));
      return false;
    }

    aValue->setObject(*JsDevices);
    return true;
  }

  void
  ReleaseMembers()
  {
    BluetoothReplyRunnable::ReleaseMembers();
    mAdapterPtr = nullptr;
  }
private:
  nsRefPtr<BluetoothAdapter> mAdapterPtr;
};

class GetScoConnectionStatusTask : public BluetoothReplyRunnable
{
public:
  GetScoConnectionStatusTask(nsIDOMDOMRequest* aReq) :
    BluetoothReplyRunnable(aReq)
  {
    MOZ_ASSERT(aReq);
  }

  virtual bool ParseSuccessfulReply(JS::Value* aValue)
  {
    *aValue = JSVAL_VOID;

    const BluetoothValue& v = mReply->get_BluetoothReplySuccess().value();
    if (v.type() != BluetoothValue::Tbool) {
      NS_WARNING("Not a boolean!");
      SetError(NS_LITERAL_STRING("BluetoothReplyTypeError"));
      return false;
    }

    aValue->setBoolean(v.get_bool());
    return true;
  }

  void
  ReleaseMembers()
  {
    BluetoothReplyRunnable::ReleaseMembers();
  }
};

static int kCreatePairedDeviceTimeout = 50000; // unit: msec

nsresult
PrepareDOMRequest(nsIDOMWindow* aWindow, nsIDOMDOMRequest** aRequest)
{
  MOZ_ASSERT(aWindow);

  nsCOMPtr<nsIDOMRequestService> rs =
    do_GetService(DOMREQUEST_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(rs, NS_ERROR_FAILURE);

  nsresult rv = rs->CreateRequest(aWindow, aRequest);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

BluetoothAdapter::BluetoothAdapter(nsPIDOMWindow* aWindow,
                                   const BluetoothValue& aValue)
  : BluetoothPropertyContainer(BluetoothObjectType::TYPE_ADAPTER)
  , mDiscoverable(false)
  , mDiscovering(false)
  , mPairable(false)
  , mPowered(false)
  , mJsUuids(nullptr)
  , mJsDeviceAddresses(nullptr)
  , mIsRooted(false)
{
  MOZ_ASSERT(aWindow);

  BindToOwner(aWindow);
  const InfallibleTArray<BluetoothNamedValue>& values =
    aValue.get_ArrayOfBluetoothNamedValue();
  for (uint32_t i = 0; i < values.Length(); ++i) {
    SetPropertyByValue(values[i]);
  }

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);
  bs->RegisterBluetoothSignalHandler(NS_LITERAL_STRING(KEY_ADAPTER), this);
}

BluetoothAdapter::~BluetoothAdapter()
{
  Unroot();
  BluetoothService* bs = BluetoothService::Get();
  // We can be null on shutdown, where this might happen
  NS_ENSURE_TRUE_VOID(bs);
  bs->UnregisterBluetoothSignalHandler(NS_LITERAL_STRING(KEY_ADAPTER), this);
}

void
BluetoothAdapter::Unroot()
{
  if (!mIsRooted) {
    return;
  }
  mJsUuids = nullptr;
  mJsDeviceAddresses = nullptr;
  NS_DROP_JS_OBJECTS(this, BluetoothAdapter);
  mIsRooted = false;
}

void
BluetoothAdapter::Root()
{
  if (mIsRooted) {
    return;
  }
  NS_HOLD_JS_OBJECTS(this, BluetoothAdapter);
  mIsRooted = true;
}

void
BluetoothAdapter::SetPropertyByValue(const BluetoothNamedValue& aValue)
{
  const nsString& name = aValue.name();
  const BluetoothValue& value = aValue.value();
  if (name.EqualsLiteral("Name")) {
    mName = value.get_nsString();
  } else if (name.EqualsLiteral("Address")) {
    mAddress = value.get_nsString();
  } else if (name.EqualsLiteral("Path")) {
    mPath = value.get_nsString();
  } else if (name.EqualsLiteral("Discoverable")) {
    mDiscoverable = value.get_bool();
  } else if (name.EqualsLiteral("Discovering")) {
    mDiscovering = value.get_bool();
  } else if (name.EqualsLiteral("Pairable")) {
    mPairable = value.get_bool();
  } else if (name.EqualsLiteral("Powered")) {
    mPowered = value.get_bool();
  } else if (name.EqualsLiteral("PairableTimeout")) {
    mPairableTimeout = value.get_uint32_t();
  } else if (name.EqualsLiteral("DiscoverableTimeout")) {
    mDiscoverableTimeout = value.get_uint32_t();
  } else if (name.EqualsLiteral("Class")) {
    mClass = value.get_uint32_t();
  } else if (name.EqualsLiteral("UUIDs")) {
    mUuids = value.get_ArrayOfnsString();
    nsresult rv;
    nsIScriptContext* sc = GetContextForEventHandlers(&rv);
    NS_ENSURE_SUCCESS_VOID(rv);

    AutoPushJSContext cx(sc->GetNativeContext());
    JS::Rooted<JSObject*> uuids(cx);
    if (NS_FAILED(nsTArrayToJSArray(cx, mUuids, uuids.address()))) {
      NS_WARNING("Cannot set JS UUIDs object!");
      return;
    }
    mJsUuids = uuids;
    Root();
  } else if (name.EqualsLiteral("Devices")) {
    mDeviceAddresses = value.get_ArrayOfnsString();

    uint32_t length = mDeviceAddresses.Length();
    for (int i = 0; i < length; i++) {
      mDeviceAddresses[i] = GetAddressFromObjectPath(mDeviceAddresses[i]);
    }

    nsresult rv;
    nsIScriptContext* sc = GetContextForEventHandlers(&rv);
    NS_ENSURE_SUCCESS_VOID(rv);

    AutoPushJSContext cx(sc->GetNativeContext());
    JS::Rooted<JSObject*> deviceAddresses(cx);
    if (NS_FAILED(nsTArrayToJSArray(cx, mDeviceAddresses,
                                    deviceAddresses.address()))) {
      NS_WARNING("Cannot set JS Devices object!");
      return;
    }
    mJsDeviceAddresses = deviceAddresses;
    Root();
  } else {
#ifdef DEBUG
    nsCString warningMsg;
    warningMsg.AssignLiteral("Not handling adapter property: ");
    warningMsg.Append(NS_ConvertUTF16toUTF8(name));
    NS_WARNING(warningMsg.get());
#endif
  }
}

// static
already_AddRefed<BluetoothAdapter>
BluetoothAdapter::Create(nsPIDOMWindow* aWindow, const BluetoothValue& aValue)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);

  nsRefPtr<BluetoothAdapter> adapter = new BluetoothAdapter(aWindow, aValue);
  return adapter.forget();
}

void
BluetoothAdapter::Notify(const BluetoothSignal& aData)
{
  InfallibleTArray<BluetoothNamedValue> arr;

  BT_LOG("[A] %s: %s", __FUNCTION__, NS_ConvertUTF16toUTF8(aData.name()).get());

  BluetoothValue v = aData.value();
  if (aData.name().EqualsLiteral("DeviceFound")) {
    nsRefPtr<BluetoothDevice> device = BluetoothDevice::Create(GetOwner(), mPath, aData.value());
    nsCOMPtr<nsIDOMEvent> event;
    NS_NewDOMBluetoothDeviceEvent(getter_AddRefs(event), this, nullptr, nullptr);

    nsCOMPtr<nsIDOMBluetoothDeviceEvent> e = do_QueryInterface(event);
    e->InitBluetoothDeviceEvent(NS_LITERAL_STRING("devicefound"),
                                false, false, device);
    DispatchTrustedEvent(event);
  } else if (aData.name().EqualsLiteral("PropertyChanged")) {
    MOZ_ASSERT(v.type() == BluetoothValue::TArrayOfBluetoothNamedValue);
    const InfallibleTArray<BluetoothNamedValue>& arr =
      v.get_ArrayOfBluetoothNamedValue();

    MOZ_ASSERT(arr.Length() == 1);
    SetPropertyByValue(arr[0]);
  } else if (aData.name().EqualsLiteral(PAIRED_STATUS_CHANGED_ID) ||
             aData.name().EqualsLiteral(HFP_STATUS_CHANGED_ID) ||
             aData.name().EqualsLiteral(SCO_STATUS_CHANGED_ID) ||
             aData.name().EqualsLiteral(A2DP_STATUS_CHANGED_ID)) {
    MOZ_ASSERT(v.type() == BluetoothValue::TArrayOfBluetoothNamedValue);
    const InfallibleTArray<BluetoothNamedValue>& arr =
      v.get_ArrayOfBluetoothNamedValue();

    MOZ_ASSERT(arr.Length() == 2 &&
               arr[0].value().type() == BluetoothValue::TnsString &&
               arr[1].value().type() == BluetoothValue::Tbool);
    nsString address = arr[0].value().get_nsString();
    bool status = arr[1].value().get_bool();

    nsCOMPtr<nsIDOMEvent> event;
    NS_NewDOMBluetoothStatusChangedEvent(
      getter_AddRefs(event), this, nullptr, nullptr);

    nsCOMPtr<nsIDOMBluetoothStatusChangedEvent> e = do_QueryInterface(event);
    e->InitBluetoothStatusChangedEvent(aData.name(), false, false,
                                       address, status);
    DispatchTrustedEvent(event);
  } else {
#ifdef DEBUG
    nsCString warningMsg;
    warningMsg.AssignLiteral("Not handling adapter signal: ");
    warningMsg.Append(NS_ConvertUTF16toUTF8(aData.name()));
    NS_WARNING(warningMsg.get());
#endif
  }
}

nsresult
BluetoothAdapter::StartStopDiscovery(bool aStart, nsIDOMDOMRequest** aRequest)
{
  nsCOMPtr<nsIDOMDOMRequest> req;
  nsresult rv;
  rv = PrepareDOMRequest(GetOwner(), getter_AddRefs(req));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  nsRefPtr<BluetoothVoidReplyRunnable> results =
    new BluetoothVoidReplyRunnable(req);

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE(bs, NS_ERROR_FAILURE);
  if (aStart) {
    rv = bs->StartDiscoveryInternal(results);
  } else {
    rv = bs->StopDiscoveryInternal(results);
  }
  if(NS_FAILED(rv)) {
    NS_WARNING("Start/Stop Discovery failed!");
    return NS_ERROR_FAILURE;
  }

  // mDiscovering is not set here, we'll get a Property update from our external
  // protocol to tell us that it's been set.

  req.forget(aRequest);
  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::StartDiscovery(nsIDOMDOMRequest** aRequest)
{
  return StartStopDiscovery(true, aRequest);
}

NS_IMETHODIMP
BluetoothAdapter::StopDiscovery(nsIDOMDOMRequest** aRequest)
{
  return StartStopDiscovery(false, aRequest);
}

NS_IMETHODIMP
BluetoothAdapter::GetAddress(nsAString& aAddress)
{
  aAddress = mAddress;
  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::GetAdapterClass(uint32_t* aClass)
{
  *aClass = mClass;
  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::GetDiscovering(bool* aDiscovering)
{
  *aDiscovering = mDiscovering;
  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::GetName(nsAString& aName)
{
  aName = mName;
  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::GetDiscoverable(bool* aDiscoverable)
{
  *aDiscoverable = mDiscoverable;
  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::GetDiscoverableTimeout(uint32_t* aDiscoverableTimeout)
{
  *aDiscoverableTimeout = mDiscoverableTimeout;
  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::GetDevices(JSContext* aCx, JS::Value* aDevices)
{
  if (mJsDeviceAddresses) {
    aDevices->setObject(*mJsDeviceAddresses);
  }
  else {
    NS_WARNING("Devices not yet set!\n");
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::GetUuids(JSContext* aCx, JS::Value* aValue)
{
  if (mJsUuids) {
    aValue->setObject(*mJsUuids);
  }
  else {
    NS_WARNING("UUIDs not yet set!\n");
    return NS_ERROR_FAILURE;
  }    
  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::SetName(const nsAString& aName,
                          nsIDOMDOMRequest** aRequest)
{
  if (mName.Equals(aName)) {
    return FirePropertyAlreadySet(GetOwner(), aRequest);
  }
  nsString name(aName);
  BluetoothValue value(name);
  BluetoothNamedValue property(NS_LITERAL_STRING("Name"), value);
  return SetProperty(GetOwner(), property, aRequest);
}

NS_IMETHODIMP
BluetoothAdapter::SetDiscoverable(const bool aDiscoverable,
                                  nsIDOMDOMRequest** aRequest)
{
  if (aDiscoverable == mDiscoverable) {
    return FirePropertyAlreadySet(GetOwner(), aRequest);
  }
  BluetoothValue value(aDiscoverable);
  BluetoothNamedValue property(NS_LITERAL_STRING("Discoverable"), value);
  return SetProperty(GetOwner(), property, aRequest);
}
 
NS_IMETHODIMP
BluetoothAdapter::SetDiscoverableTimeout(const uint32_t aDiscoverableTimeout,
                                         nsIDOMDOMRequest** aRequest)
{
  if (aDiscoverableTimeout == mDiscoverableTimeout) {
    return FirePropertyAlreadySet(GetOwner(), aRequest);
  }
  BluetoothValue value(aDiscoverableTimeout);
  BluetoothNamedValue property(NS_LITERAL_STRING("DiscoverableTimeout"), value);
  return SetProperty(GetOwner(), property, aRequest);
}

NS_IMETHODIMP
BluetoothAdapter::GetConnectedDevices(uint16_t aProfileId,
                                      nsIDOMDOMRequest** aRequest)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIDOMDOMRequest> req;
  nsresult rv = PrepareDOMRequest(GetOwner(), getter_AddRefs(req));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  nsRefPtr<BluetoothReplyRunnable> results =
    new GetDevicesTask(this, req);

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE(bs, NS_ERROR_FAILURE);
  rv = bs->GetConnectedDevicePropertiesInternal(aProfileId, results);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  req.forget(aRequest);
  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::GetPairedDevices(nsIDOMDOMRequest** aRequest)
{
  nsCOMPtr<nsIDOMDOMRequest> req;
  nsresult rv = PrepareDOMRequest(GetOwner(), getter_AddRefs(req));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  nsRefPtr<BluetoothReplyRunnable> results =
    new GetDevicesTask(this, req);

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE(bs, NS_ERROR_FAILURE);
  rv = bs->GetPairedDevicePropertiesInternal(mDeviceAddresses, results);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  req.forget(aRequest);
  return NS_OK;
}

nsresult
BluetoothAdapter::PairUnpair(bool aPair,
                             nsIDOMBluetoothDevice* aDevice,
                             nsIDOMDOMRequest** aRequest)
{
  nsCOMPtr<nsIDOMDOMRequest> req;
  nsresult rv = PrepareDOMRequest(GetOwner(), getter_AddRefs(req));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  nsRefPtr<BluetoothVoidReplyRunnable> results =
    new BluetoothVoidReplyRunnable(req);

  nsAutoString addr;
  aDevice->GetAddress(addr);

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE(bs, NS_ERROR_FAILURE);
  if (aPair) {
    rv = bs->CreatePairedDeviceInternal(addr,
                                        kCreatePairedDeviceTimeout,
                                        results);
  } else {
    rv = bs->RemoveDeviceInternal(addr, results);
  }

  if (NS_FAILED(rv)) {
    NS_WARNING("Pair/Unpair failed!");
    return NS_ERROR_FAILURE;
  }

  req.forget(aRequest);
  return NS_OK;
}

nsresult
BluetoothAdapter::Pair(nsIDOMBluetoothDevice* aDevice,
                       nsIDOMDOMRequest** aRequest)
{
  return PairUnpair(true, aDevice, aRequest);
}

nsresult
BluetoothAdapter::Unpair(nsIDOMBluetoothDevice* aDevice,
                         nsIDOMDOMRequest** aRequest)
{
  return PairUnpair(false, aDevice, aRequest);
}

nsresult
BluetoothAdapter::SetPinCode(const nsAString& aDeviceAddress,
                             const nsAString& aPinCode,
                             nsIDOMDOMRequest** aRequest)
{
  nsCOMPtr<nsIDOMDOMRequest> req;
  nsresult rv = PrepareDOMRequest(GetOwner(), getter_AddRefs(req));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  nsRefPtr<BluetoothVoidReplyRunnable> results =
    new BluetoothVoidReplyRunnable(req);

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE(bs, NS_ERROR_FAILURE);
  if(!bs->SetPinCodeInternal(aDeviceAddress, aPinCode, results)) {
    NS_WARNING("SetPinCode failed!");
    return NS_ERROR_FAILURE;
  }

  req.forget(aRequest);
  return NS_OK;
}

nsresult
BluetoothAdapter::SetPasskey(const nsAString& aDeviceAddress, uint32_t aPasskey,
                             nsIDOMDOMRequest** aRequest)
{
  nsCOMPtr<nsIDOMDOMRequest> req;
  nsresult rv = PrepareDOMRequest(GetOwner(), getter_AddRefs(req));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  nsRefPtr<BluetoothVoidReplyRunnable> results =
    new BluetoothVoidReplyRunnable(req);

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE(bs, NS_ERROR_FAILURE);
  if(bs->SetPasskeyInternal(aDeviceAddress, aPasskey, results)) {
    NS_WARNING("SetPasskeyInternal failed!");
    return NS_ERROR_FAILURE;
  }

  req.forget(aRequest);
  return NS_OK;
}

nsresult
BluetoothAdapter::SetPairingConfirmation(const nsAString& aDeviceAddress,
                                         bool aConfirmation,
                                         nsIDOMDOMRequest** aRequest)
{
  nsCOMPtr<nsIDOMDOMRequest> req;
  nsresult rv = PrepareDOMRequest(GetOwner(), getter_AddRefs(req));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  nsRefPtr<BluetoothVoidReplyRunnable> results =
    new BluetoothVoidReplyRunnable(req);

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE(bs, NS_ERROR_FAILURE);
  if(!bs->SetPairingConfirmationInternal(aDeviceAddress,
                                         aConfirmation,
                                         results)) {
    NS_WARNING("SetPairingConfirmation failed!");
    return NS_ERROR_FAILURE;
  }

  req.forget(aRequest);
  return NS_OK;
}

nsresult
BluetoothAdapter::SetAuthorization(const nsAString& aDeviceAddress, bool aAllow,
                                   nsIDOMDOMRequest** aRequest)
{
  nsCOMPtr<nsIDOMDOMRequest> req;
  nsresult rv = PrepareDOMRequest(GetOwner(), getter_AddRefs(req));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  nsRefPtr<BluetoothVoidReplyRunnable> results =
    new BluetoothVoidReplyRunnable(req);

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE(bs, NS_ERROR_FAILURE);
  if(!bs->SetAuthorizationInternal(aDeviceAddress, aAllow, results)) {
    NS_WARNING("SetAuthorization failed!");
    return NS_ERROR_FAILURE;
  }

  req.forget(aRequest);
  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::Connect(const nsAString& aDeviceAddress,
                          uint16_t aProfileId,
                          nsIDOMDOMRequest** aRequest)
{
  nsCOMPtr<nsIDOMDOMRequest> req;
  nsresult rv = PrepareDOMRequest(GetOwner(), getter_AddRefs(req));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE(bs, NS_ERROR_FAILURE);

  nsRefPtr<BluetoothVoidReplyRunnable> results =
    new BluetoothVoidReplyRunnable(req);
  bs->Connect(aDeviceAddress, aProfileId, results);

  req.forget(aRequest);
  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::Disconnect(uint16_t aProfileId,
                             nsIDOMDOMRequest** aRequest)
{
  nsCOMPtr<nsIDOMDOMRequest> req;
  nsresult rv = PrepareDOMRequest(GetOwner(), getter_AddRefs(req));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  nsRefPtr<BluetoothVoidReplyRunnable> results =
    new BluetoothVoidReplyRunnable(req);

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE(bs, NS_ERROR_FAILURE);
  bs->Disconnect(aProfileId, results);

  req.forget(aRequest);
  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::SendFile(const nsAString& aDeviceAddress,
                           nsIDOMBlob* aBlob,
                           nsIDOMDOMRequest** aRequest)
{
  nsCOMPtr<nsIDOMDOMRequest> req;
  nsresult rv = PrepareDOMRequest(GetOwner(), getter_AddRefs(req));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  nsRefPtr<BluetoothVoidReplyRunnable> results =
    new BluetoothVoidReplyRunnable(req);

  BlobChild* actor =
    mozilla::dom::ContentChild::GetSingleton()->GetOrCreateActorForBlob(aBlob);
  if (!actor) {
    NS_WARNING("Can't create actor");
    return NS_ERROR_FAILURE;
  }

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE(bs, NS_ERROR_FAILURE);
  bs->SendFile(aDeviceAddress, nullptr, actor, results);

  req.forget(aRequest);
  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::StopSendingFile(const nsAString& aDeviceAddress,
                                  nsIDOMDOMRequest** aRequest)
{
  nsCOMPtr<nsIDOMDOMRequest> req;
  nsresult rv = PrepareDOMRequest(GetOwner(), getter_AddRefs(req));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  nsRefPtr<BluetoothVoidReplyRunnable> results =
    new BluetoothVoidReplyRunnable(req);

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE(bs, NS_ERROR_FAILURE);
  bs->StopSendingFile(aDeviceAddress, results);

  req.forget(aRequest);
  return NS_OK;
}

nsresult
BluetoothAdapter::ConfirmReceivingFile(const nsAString& aDeviceAddress,
                                       bool aConfirmation,
                                       nsIDOMDOMRequest** aRequest)
{
  nsCOMPtr<nsIDOMDOMRequest> req;
  nsresult rv = PrepareDOMRequest(GetOwner(), getter_AddRefs(req));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  nsRefPtr<BluetoothVoidReplyRunnable> results =
    new BluetoothVoidReplyRunnable(req);

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE(bs, NS_ERROR_FAILURE);
  bs->ConfirmReceivingFile(aDeviceAddress, aConfirmation, results);

  req.forget(aRequest);
  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::ConnectSco(nsIDOMDOMRequest** aRequest)
{
  nsCOMPtr<nsIDOMDOMRequest> req;
  nsresult rv = PrepareDOMRequest(GetOwner(), getter_AddRefs(req));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  nsRefPtr<BluetoothVoidReplyRunnable> results =
    new BluetoothVoidReplyRunnable(req);

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE(bs, NS_ERROR_FAILURE);
  bs->ConnectSco(results);

  req.forget(aRequest);
  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::DisconnectSco(nsIDOMDOMRequest** aRequest)
{
  nsCOMPtr<nsIDOMDOMRequest> req;
  nsresult rv = PrepareDOMRequest(GetOwner(), getter_AddRefs(req));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  nsRefPtr<BluetoothVoidReplyRunnable> results =
    new BluetoothVoidReplyRunnable(req);

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE(bs, NS_ERROR_FAILURE);
  bs->DisconnectSco(results);

  req.forget(aRequest);
  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::IsScoConnected(nsIDOMDOMRequest** aRequest)
{
  nsCOMPtr<nsIDOMDOMRequest> req;
  nsresult rv = PrepareDOMRequest(GetOwner(), getter_AddRefs(req));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  nsRefPtr<BluetoothReplyRunnable> results =
    new GetScoConnectionStatusTask(req);

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE(bs, NS_ERROR_FAILURE);
  bs->IsScoConnected(results);

  req.forget(aRequest);
  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::SendMediaMetaData(const JS::Value& aOptions,
                                    nsIDOMDOMRequest** aRequest)
{
  MediaMetaData metadata;

  nsresult rv;
  nsIScriptContext* sc = GetContextForEventHandlers(&rv);
  NS_ENSURE_SUCCESS(rv, rv);

  AutoPushJSContext cx(sc->GetNativeContext());
  rv = metadata.Init(cx, &aOptions);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMDOMRequest> req;
  rv = PrepareDOMRequest(GetOwner(), getter_AddRefs(req));
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<BluetoothReplyRunnable> results =
    new BluetoothVoidReplyRunnable(req);

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE(bs, NS_ERROR_FAILURE);
  bs->SendMetaData(metadata.mTitle,
                   metadata.mArtist,
                   metadata.mAlbum,
                   metadata.mMediaNumber,
                   metadata.mTotalMediaCount,
                   metadata.mDuration,
                   results);

  req.forget(aRequest);
  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::SendMediaPlayStatus(const JS::Value& aOptions,
                                      nsIDOMDOMRequest** aRequest)
{
  MediaPlayStatus status;

  nsresult rv;
  nsIScriptContext* sc = GetContextForEventHandlers(&rv);
  NS_ENSURE_SUCCESS(rv, rv);

  AutoPushJSContext cx(sc->GetNativeContext());
  rv = status.Init(cx, &aOptions);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMDOMRequest> req;
  rv = PrepareDOMRequest(GetOwner(), getter_AddRefs(req));
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<BluetoothReplyRunnable> results =
    new BluetoothVoidReplyRunnable(req);

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE(bs, NS_ERROR_FAILURE);
  bs->SendPlayStatus(status.mDuration,
                     status.mPosition,
                     status.mPlayStatus,
                     results);

  req.forget(aRequest);
  return NS_OK;
}

NS_IMPL_EVENT_HANDLER(BluetoothAdapter, devicefound)
NS_IMPL_EVENT_HANDLER(BluetoothAdapter, a2dpstatuschanged)
NS_IMPL_EVENT_HANDLER(BluetoothAdapter, hfpstatuschanged)
NS_IMPL_EVENT_HANDLER(BluetoothAdapter, pairedstatuschanged)
NS_IMPL_EVENT_HANDLER(BluetoothAdapter, scostatuschanged)
