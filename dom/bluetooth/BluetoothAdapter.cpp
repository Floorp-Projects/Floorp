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
#include "mozilla/dom/BluetoothAdapterBinding.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/LazyIdleThread.h"
#include "mozilla/Util.h"

#include "BluetoothAdapter.h"
#include "BluetoothDevice.h"
#include "BluetoothReplyRunnable.h"
#include "BluetoothService.h"
#include "BluetoothUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

USING_BLUETOOTH_NAMESPACE

NS_IMPL_CYCLE_COLLECTION_CLASS(BluetoothAdapter)

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

// QueryInterface implementation for BluetoothAdapter
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(BluetoothAdapter)
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

BluetoothAdapter::BluetoothAdapter(nsPIDOMWindow* aWindow,
                                   const BluetoothValue& aValue)
  : nsDOMEventTargetHelper(aWindow)
  , BluetoothPropertyContainer(BluetoothObjectType::TYPE_ADAPTER)
  , mJsUuids(nullptr)
  , mJsDeviceAddresses(nullptr)
  , mDiscoverable(false)
  , mDiscovering(false)
  , mPairable(false)
  , mPowered(false)
  , mIsRooted(false)
{
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(IsDOMBinding());

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

already_AddRefed<DOMRequest>
BluetoothAdapter::StartStopDiscovery(bool aStart, ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(win);
  nsRefPtr<BluetoothVoidReplyRunnable> results =
    new BluetoothVoidReplyRunnable(request);

  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  nsresult rv;
  if (aStart) {
    rv = bs->StartDiscoveryInternal(results);
  } else {
    rv = bs->StopDiscoveryInternal(results);
  }
  if (NS_FAILED(rv)) {
    NS_WARNING("Start/Stop Discovery failed!");
    aRv.Throw(rv);
    return nullptr;
  }

  // mDiscovering is not set here, we'll get a Property update from our external
  // protocol to tell us that it's been set.

  return request.forget();
}

already_AddRefed<DOMRequest>
BluetoothAdapter::StartDiscovery(ErrorResult& aRv)
{
  return StartStopDiscovery(true, aRv);
}

already_AddRefed<DOMRequest>
BluetoothAdapter::StopDiscovery(ErrorResult& aRv)
{
  return StartStopDiscovery(false, aRv);
}

JS::Value
BluetoothAdapter::GetDevices(JSContext* aContext, ErrorResult& aRv)
{
  if (!mJsDeviceAddresses) {
    NS_WARNING("Devices not yet set!\n");
    aRv.Throw(NS_ERROR_FAILURE);
    return JS::NullValue();
  }

  return JS::ObjectValue(*xpc_UnmarkGrayObject(mJsDeviceAddresses));
}

JS::Value
BluetoothAdapter::GetUuids(JSContext* aContext, ErrorResult& aRv)
{
  if (!mJsUuids) {
    NS_WARNING("UUIDs not yet set!\n");
    aRv.Throw(NS_ERROR_FAILURE);
    return JS::NullValue();
  }

  return JS::ObjectValue(*xpc_UnmarkGrayObject(mJsUuids));
}

already_AddRefed<DOMRequest>
BluetoothAdapter::SetName(const nsAString& aName, ErrorResult& aRv)
{
  if (mName.Equals(aName)) {
    return FirePropertyAlreadySet(GetOwner(), aRv);
  }
  nsString name(aName);
  BluetoothValue value(name);
  BluetoothNamedValue property(NS_LITERAL_STRING("Name"), value);
  return SetProperty(GetOwner(), property, aRv);
}

already_AddRefed<DOMRequest>
BluetoothAdapter::SetDiscoverable(bool aDiscoverable, ErrorResult& aRv)
{
  if (aDiscoverable == mDiscoverable) {
    return FirePropertyAlreadySet(GetOwner(), aRv);
  }
  BluetoothValue value(aDiscoverable);
  BluetoothNamedValue property(NS_LITERAL_STRING("Discoverable"), value);
  return SetProperty(GetOwner(), property, aRv);
}

already_AddRefed<DOMRequest>
BluetoothAdapter::SetDiscoverableTimeout(uint32_t aDiscoverableTimeout, ErrorResult& aRv)
{
  if (aDiscoverableTimeout == mDiscoverableTimeout) {
    return FirePropertyAlreadySet(GetOwner(), aRv);
  }
  BluetoothValue value(aDiscoverableTimeout);
  BluetoothNamedValue property(NS_LITERAL_STRING("DiscoverableTimeout"), value);
  return SetProperty(GetOwner(), property, aRv);
}

already_AddRefed<DOMRequest>
BluetoothAdapter::GetConnectedDevices(uint16_t aProfileId, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(win);
  nsRefPtr<BluetoothReplyRunnable> results =
    new GetDevicesTask(this, request);

  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  nsresult rv = bs->GetConnectedDevicePropertiesInternal(aProfileId, results);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<DOMRequest>
BluetoothAdapter::GetPairedDevices(ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(win);
  nsRefPtr<BluetoothReplyRunnable> results =
    new GetDevicesTask(this, request);

  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  nsresult rv = bs->GetPairedDevicePropertiesInternal(mDeviceAddresses, results);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<DOMRequest>
BluetoothAdapter::PairUnpair(bool aPair, BluetoothDevice& aDevice,
                             ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(win);
  nsRefPtr<BluetoothVoidReplyRunnable> results =
    new BluetoothVoidReplyRunnable(request);

  nsAutoString addr;
  aDevice.GetAddress(addr);

  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  nsresult rv;
  if (aPair) {
    rv = bs->CreatePairedDeviceInternal(addr,
                                        kCreatePairedDeviceTimeout,
                                        results);
  } else {
    rv = bs->RemoveDeviceInternal(addr, results);
  }
  if (NS_FAILED(rv)) {
    NS_WARNING("Pair/Unpair failed!");
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<DOMRequest>
BluetoothAdapter::Pair(BluetoothDevice& aDevice, ErrorResult& aRv)
{
  return PairUnpair(true, aDevice, aRv);
}

already_AddRefed<DOMRequest>
BluetoothAdapter::Unpair(BluetoothDevice& aDevice, ErrorResult& aRv)
{
  return PairUnpair(false, aDevice, aRv);
}

already_AddRefed<DOMRequest>
BluetoothAdapter::SetPinCode(const nsAString& aDeviceAddress,
                             const nsAString& aPinCode, ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(win);
  nsRefPtr<BluetoothVoidReplyRunnable> results =
    new BluetoothVoidReplyRunnable(request);

  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  if (!bs->SetPinCodeInternal(aDeviceAddress, aPinCode, results)) {
    NS_WARNING("SetPinCode failed!");
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<DOMRequest>
BluetoothAdapter::SetPasskey(const nsAString& aDeviceAddress, uint32_t aPasskey,
                             ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(win);
  nsRefPtr<BluetoothVoidReplyRunnable> results =
    new BluetoothVoidReplyRunnable(request);

  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  if (bs->SetPasskeyInternal(aDeviceAddress, aPasskey, results)) {
    NS_WARNING("SetPasskeyInternal failed!");
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<DOMRequest>
BluetoothAdapter::SetPairingConfirmation(const nsAString& aDeviceAddress,
                                         bool aConfirmation, ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(win);
  nsRefPtr<BluetoothVoidReplyRunnable> results =
    new BluetoothVoidReplyRunnable(request);

  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  if (!bs->SetPairingConfirmationInternal(aDeviceAddress,
                                          aConfirmation,
                                          results)) {
    NS_WARNING("SetPairingConfirmation failed!");
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<DOMRequest>
BluetoothAdapter::Connect(const nsAString& aDeviceAddress,
                          uint16_t aProfileId, ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(win);
  nsRefPtr<BluetoothVoidReplyRunnable> results =
    new BluetoothVoidReplyRunnable(request);

  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  bs->Connect(aDeviceAddress, aProfileId, results);

  return request.forget();
}

already_AddRefed<DOMRequest>
BluetoothAdapter::Disconnect(uint16_t aProfileId, ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(win);
  nsRefPtr<BluetoothVoidReplyRunnable> results =
    new BluetoothVoidReplyRunnable(request);

  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  bs->Disconnect(aProfileId, results);

  return request.forget();
}

already_AddRefed<DOMRequest>
BluetoothAdapter::SendFile(const nsAString& aDeviceAddress,
                           nsIDOMBlob* aBlob, ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(win);
  nsRefPtr<BluetoothVoidReplyRunnable> results =
    new BluetoothVoidReplyRunnable(request);

  BlobChild* actor =
    ContentChild::GetSingleton()->GetOrCreateActorForBlob(aBlob);
  if (!actor) {
    NS_WARNING("Can't create actor");
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  bs->SendFile(aDeviceAddress, nullptr, actor, results);

  return request.forget();
}

already_AddRefed<DOMRequest>
BluetoothAdapter::StopSendingFile(const nsAString& aDeviceAddress, ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(win);
  nsRefPtr<BluetoothVoidReplyRunnable> results =
    new BluetoothVoidReplyRunnable(request);

  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  bs->StopSendingFile(aDeviceAddress, results);

  return request.forget();
}

already_AddRefed<DOMRequest>
BluetoothAdapter::ConfirmReceivingFile(const nsAString& aDeviceAddress,
                                       bool aConfirmation, ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(win);
  nsRefPtr<BluetoothVoidReplyRunnable> results =
    new BluetoothVoidReplyRunnable(request);

  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  bs->ConfirmReceivingFile(aDeviceAddress, aConfirmation, results);

  return request.forget();
}

already_AddRefed<DOMRequest>
BluetoothAdapter::ConnectSco(ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(win);
  nsRefPtr<BluetoothVoidReplyRunnable> results =
    new BluetoothVoidReplyRunnable(request);

  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  bs->ConnectSco(results);

  return request.forget();
}

already_AddRefed<DOMRequest>
BluetoothAdapter::DisconnectSco(ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(win);
  nsRefPtr<BluetoothVoidReplyRunnable> results =
    new BluetoothVoidReplyRunnable(request);

  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  bs->DisconnectSco(results);

  return request.forget();
}

already_AddRefed<DOMRequest>
BluetoothAdapter::IsScoConnected(ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(win);
  nsRefPtr<BluetoothReplyRunnable> results =
    new GetScoConnectionStatusTask(request);

  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  bs->IsScoConnected(results);

  return request.forget();
}

already_AddRefed<DOMRequest>
BluetoothAdapter::SendMediaMetaData(const MediaMetaData& aMediaMetaData, ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(win);
  nsRefPtr<BluetoothReplyRunnable> results =
    new BluetoothVoidReplyRunnable(request);

  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  bs->SendMetaData(aMediaMetaData.mTitle,
                   aMediaMetaData.mArtist,
                   aMediaMetaData.mAlbum,
                   aMediaMetaData.mMediaNumber,
                   aMediaMetaData.mTotalMediaCount,
                   aMediaMetaData.mDuration,
                   results);

  return request.forget();
}

already_AddRefed<DOMRequest>
BluetoothAdapter::SendMediaPlayStatus(const MediaPlayStatus& aMediaPlayStatus, ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(win);
  nsRefPtr<BluetoothReplyRunnable> results =
    new BluetoothVoidReplyRunnable(request);

  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  bs->SendPlayStatus(aMediaPlayStatus.mDuration,
                     aMediaPlayStatus.mPosition,
                     aMediaPlayStatus.mPlayStatus,
                     results);

  return request.forget();
}

JSObject*
BluetoothAdapter::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return BluetoothAdapterBinding::Wrap(aCx, aScope, this);
}
