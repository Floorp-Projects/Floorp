/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"
#include "nsDOMClassInfo.h"
#include "nsTArrayHelpers.h"
#include "DOMRequest.h"
#include "nsThreadUtils.h"

#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "mozilla/dom/BluetoothAdapter2Binding.h"
#include "mozilla/dom/BluetoothAttributeEvent.h"
#include "mozilla/dom/BluetoothDeviceEvent.h"
#include "mozilla/dom/BluetoothStatusChangedEvent.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/LazyIdleThread.h"

#include "BluetoothAdapter.h"
#include "BluetoothClassOfDevice.h"
#include "BluetoothDevice.h"
#include "BluetoothDiscoveryHandle.h"
#include "BluetoothReplyRunnable.h"
#include "BluetoothService.h"
#include "BluetoothUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

USING_BLUETOOTH_NAMESPACE

NS_IMPL_CYCLE_COLLECTION_CLASS(BluetoothAdapter)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(BluetoothAdapter,
                                               DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mJsUuids)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mJsDeviceAddresses)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(BluetoothAdapter,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(BluetoothAdapter,
                                                DOMEventTargetHelper)
  tmp->Unroot();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

// QueryInterface implementation for BluetoothAdapter
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(BluetoothAdapter)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(BluetoothAdapter, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(BluetoothAdapter, DOMEventTargetHelper)

class StartDiscoveryTask : public BluetoothReplyRunnable
{
 public:
  StartDiscoveryTask(BluetoothAdapter* aAdapter, Promise* aPromise)
    : BluetoothReplyRunnable(nullptr, aPromise,
                             NS_LITERAL_STRING("StartDiscovery"))
    , mAdapter(aAdapter)
  {
    MOZ_ASSERT(aPromise);
    MOZ_ASSERT(aAdapter);
  }

  bool
  ParseSuccessfulReply(JS::MutableHandle<JS::Value> aValue)
  {
    aValue.setUndefined();

    AutoJSAPI jsapi;
    NS_ENSURE_TRUE(jsapi.Init(mAdapter->GetParentObject()), false);
    JSContext* cx = jsapi.cx();

    /**
     * Create a new discovery handle and wrap it to return. Each
     * discovery handle is one-time-use only.
     */
    nsRefPtr<BluetoothDiscoveryHandle> discoveryHandle =
      BluetoothDiscoveryHandle::Create(mAdapter->GetParentObject());
    if (!ToJSValue(cx, discoveryHandle, aValue)) {
      JS_ClearPendingException(cx);
      return false;
    }

    // Set the created discovery handle as the one in use.
    mAdapter->SetDiscoveryHandleInUse(discoveryHandle);
    return true;
  }

  virtual void
  ReleaseMembers() MOZ_OVERRIDE
  {
    BluetoothReplyRunnable::ReleaseMembers();
    mAdapter = nullptr;
  }

private:
  nsRefPtr<BluetoothAdapter> mAdapter;
};

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

  virtual bool ParseSuccessfulReply(JS::MutableHandle<JS::Value> aValue)
  {
    aValue.setUndefined();

    const BluetoothValue& v = mReply->get_BluetoothReplySuccess().value();
    if (v.type() != BluetoothValue::TArrayOfBluetoothNamedValue) {
      BT_WARNING("Not a BluetoothNamedValue array!");
      SetError(NS_LITERAL_STRING("BluetoothReplyTypeError"));
      return false;
    }

    const InfallibleTArray<BluetoothNamedValue>& values =
      v.get_ArrayOfBluetoothNamedValue();

    nsTArray<nsRefPtr<BluetoothDevice> > devices;
    for (uint32_t i = 0; i < values.Length(); i++) {
      const BluetoothValue properties = values[i].value();
      if (properties.type() != BluetoothValue::TArrayOfBluetoothNamedValue) {
        BT_WARNING("Not a BluetoothNamedValue array!");
        SetError(NS_LITERAL_STRING("BluetoothReplyTypeError"));
        return false;
      }
      nsRefPtr<BluetoothDevice> d =
        BluetoothDevice::Create(mAdapterPtr->GetOwner(),
                                properties);
      devices.AppendElement(d);
    }

    AutoJSAPI jsapi;
    if (!jsapi.Init(mAdapterPtr->GetOwner())) {
      BT_WARNING("Failed to initialise AutoJSAPI!");
      SetError(NS_LITERAL_STRING("BluetoothAutoJSAPIInitError"));
      return false;
    }
    JSContext* cx = jsapi.cx();
    JS::Rooted<JSObject*> JsDevices(cx);
    if (NS_FAILED(nsTArrayToJSArray(cx, devices, &JsDevices))) {
      BT_WARNING("Cannot create JS array!");
      SetError(NS_LITERAL_STRING("BluetoothError"));
      return false;
    }

    aValue.setObject(*JsDevices);
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

  virtual bool ParseSuccessfulReply(JS::MutableHandle<JS::Value> aValue)
  {
    aValue.setUndefined();

    const BluetoothValue& v = mReply->get_BluetoothReplySuccess().value();
    if (v.type() != BluetoothValue::Tbool) {
      BT_WARNING("Not a boolean!");
      SetError(NS_LITERAL_STRING("BluetoothReplyTypeError"));
      return false;
    }

    aValue.setBoolean(v.get_bool());
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
  : DOMEventTargetHelper(aWindow)
  , mJsUuids(nullptr)
  , mJsDeviceAddresses(nullptr)
  , mDiscoveryHandleInUse(nullptr)
  , mState(BluetoothAdapterState::Disabled)
  , mDiscoverable(false)
  , mDiscovering(false)
  , mPairable(false)
  , mPowered(false)
  , mIsRooted(false)
{
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(IsDOMBinding());

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
BluetoothAdapter::DisconnectFromOwner()
{
  DOMEventTargetHelper::DisconnectFromOwner();

  BluetoothService* bs = BluetoothService::Get();
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
  mozilla::DropJSObjects(this);
  mIsRooted = false;
}

void
BluetoothAdapter::Root()
{
  if (mIsRooted) {
    return;
  }
  mozilla::HoldJSObjects(this);
  mIsRooted = true;
}

void
BluetoothAdapter::SetPropertyByValue(const BluetoothNamedValue& aValue)
{
  const nsString& name = aValue.name();
  const BluetoothValue& value = aValue.value();
  if (name.EqualsLiteral("State")) {
    mState = value.get_bool() ? BluetoothAdapterState::Enabled
                              : BluetoothAdapterState::Disabled;
  } else if (name.EqualsLiteral("Name")) {
    mName = value.get_nsString();
  } else if (name.EqualsLiteral("Address")) {
    mAddress = value.get_nsString();
  } else if (name.EqualsLiteral("Discoverable")) {
    mDiscoverable = value.get_bool();
  } else if (name.EqualsLiteral("Discovering")) {
    mDiscovering = value.get_bool();
    if (!mDiscovering) {
      // Reset discovery handle in use to nullptr
      SetDiscoveryHandleInUse(nullptr);
    }
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

    AutoJSAPI jsapi;
    if (!jsapi.Init(GetOwner())) {
      BT_WARNING("Failed to initialise AutoJSAPI!");
      return;
    }
    JSContext* cx = jsapi.cx();
    JS::Rooted<JSObject*> uuids(cx);
    if (NS_FAILED(nsTArrayToJSArray(cx, mUuids, &uuids))) {
      BT_WARNING("Cannot set JS UUIDs object!");
      return;
    }
    mJsUuids = uuids;
    Root();
  } else if (name.EqualsLiteral("Devices")) {
    mDeviceAddresses = value.get_ArrayOfnsString();

    AutoJSAPI jsapi;
    if (!jsapi.Init(GetOwner())) {
      BT_WARNING("Failed to initialise AutoJSAPI!");
      return;
    }
    JSContext* cx = jsapi.cx();
    JS::Rooted<JSObject*> deviceAddresses(cx);
    if (NS_FAILED(nsTArrayToJSArray(cx, mDeviceAddresses, &deviceAddresses))) {
      BT_WARNING("Cannot set JS Devices object!");
      return;
    }
    mJsDeviceAddresses = deviceAddresses;
    Root();
  } else {
    BT_WARNING("Not handling adapter property: %s",
               NS_ConvertUTF16toUTF8(name).get());
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

  BT_LOGD("[A] %s", NS_ConvertUTF16toUTF8(aData.name()).get());

  BluetoothValue v = aData.value();
  if (aData.name().EqualsLiteral("PropertyChanged")) {
    HandlePropertyChanged(v);
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

    BluetoothStatusChangedEventInit init;
    init.mBubbles = false;
    init.mCancelable = false;
    init.mAddress = address;
    init.mStatus = status;
    nsRefPtr<BluetoothStatusChangedEvent> event =
      BluetoothStatusChangedEvent::Constructor(this, aData.name(), init);
    DispatchTrustedEvent(event);
  } else if (aData.name().EqualsLiteral(REQUEST_MEDIA_PLAYSTATUS_ID)) {
    nsCOMPtr<nsIDOMEvent> event;
    nsresult rv = NS_NewDOMEvent(getter_AddRefs(event), this, nullptr, nullptr);
    NS_ENSURE_SUCCESS_VOID(rv);

    rv = event->InitEvent(aData.name(), false, false);
    NS_ENSURE_SUCCESS_VOID(rv);

    DispatchTrustedEvent(event);
  } else {
    BT_WARNING("Not handling adapter signal: %s",
               NS_ConvertUTF16toUTF8(aData.name()).get());
  }
}

void
BluetoothAdapter::SetDiscoveryHandleInUse(
  BluetoothDiscoveryHandle* aDiscoveryHandle)
{
  // Stop discovery handle in use from listening to "DeviceFound" signal
  if (mDiscoveryHandleInUse) {
    mDiscoveryHandleInUse->DisconnectFromOwner();
  }

  mDiscoveryHandleInUse = aDiscoveryHandle;
}

already_AddRefed<Promise>
BluetoothAdapter::StartDiscovery(ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  nsRefPtr<Promise> promise = new Promise(global);

  /**
   * Ensure
   * - adapter is not discovering (note we reject here to ensure
       each resolved promise returns a new BluetoothDiscoveryHandle),
   * - adapter is already enabled, and
   * - BluetoothService is available
   */
  BT_ENSURE_TRUE_REJECT(!mDiscovering, NS_ERROR_DOM_INVALID_STATE_ERR);
  BT_ENSURE_TRUE_REJECT(mState == BluetoothAdapterState::Enabled,
                        NS_ERROR_DOM_INVALID_STATE_ERR);
  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, NS_ERROR_NOT_AVAILABLE);

  BT_API2_LOGR();

  // Return BluetoothDiscoveryHandle in StartDiscoveryTask
  nsRefPtr<BluetoothReplyRunnable> result =
    new StartDiscoveryTask(this, promise);
  BT_ENSURE_TRUE_REJECT(NS_SUCCEEDED(bs->StartDiscoveryInternal(result)),
                        NS_ERROR_DOM_OPERATION_ERR);

  return promise.forget();
}

already_AddRefed<Promise>
BluetoothAdapter::StopDiscovery(ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  nsRefPtr<Promise> promise = new Promise(global);

  /**
   * Ensure
   * - adapter is discovering,
   * - adapter is already enabled, and
   * - BluetoothService is available
   */
  BT_ENSURE_TRUE_RESOLVE(mDiscovering, JS::UndefinedHandleValue);
  BT_ENSURE_TRUE_REJECT(mState == BluetoothAdapterState::Enabled,
                        NS_ERROR_DOM_INVALID_STATE_ERR);
  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, NS_ERROR_NOT_AVAILABLE);

  BT_API2_LOGR();

  nsRefPtr<BluetoothReplyRunnable> result =
    new BluetoothVoidReplyRunnable(nullptr /* DOMRequest */,
                                   promise,
                                   NS_LITERAL_STRING("StopDiscovery"));
  BT_ENSURE_TRUE_REJECT(NS_SUCCEEDED(bs->StopDiscoveryInternal(result)),
                        NS_ERROR_DOM_OPERATION_ERR);

  return promise.forget();
}

void
BluetoothAdapter::GetDevices(JSContext* aContext,
                             JS::MutableHandle<JS::Value> aDevices,
                             ErrorResult& aRv)
{
  if (!mJsDeviceAddresses) {
    BT_WARNING("Devices not yet set!\n");
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  JS::ExposeObjectToActiveJS(mJsDeviceAddresses);
  aDevices.setObject(*mJsDeviceAddresses);
}

void
BluetoothAdapter::GetUuids(JSContext* aContext,
                           JS::MutableHandle<JS::Value> aUuids,
                           ErrorResult& aRv)
{
  if (!mJsUuids) {
    BT_WARNING("UUIDs not yet set!\n");
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  JS::ExposeObjectToActiveJS(mJsUuids);
  aUuids.setObject(*mJsUuids);
}

already_AddRefed<Promise>
BluetoothAdapter::SetName(const nsAString& aName, ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if(!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  nsRefPtr<Promise> promise = new Promise(global);

  /**
   * Ensure
   * - adapter's name does not equal to aName,
   * - adapter is already enabled, and
   * - BluetoothService is available
   */
  BT_ENSURE_TRUE_RESOLVE(!mName.Equals(aName), JS::UndefinedHandleValue);
  BT_ENSURE_TRUE_REJECT(mState == BluetoothAdapterState::Enabled,
                        NS_ERROR_DOM_INVALID_STATE_ERR);
  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, NS_ERROR_NOT_AVAILABLE);

  // Wrap property to set and runnable to handle result
  nsString name(aName);
  BluetoothNamedValue property(NS_LITERAL_STRING("Name"),
                               BluetoothValue(name));
  nsRefPtr<BluetoothReplyRunnable> result =
    new BluetoothVoidReplyRunnable(nullptr /* DOMRequest */,
                                   promise,
                                   NS_LITERAL_STRING("SetName"));
  BT_ENSURE_TRUE_REJECT(
    NS_SUCCEEDED(bs->SetProperty(BluetoothObjectType::TYPE_ADAPTER,
                                 property, result)),
    NS_ERROR_DOM_OPERATION_ERR);

  return promise.forget();
}

already_AddRefed<Promise>
BluetoothAdapter::SetDiscoverable(bool aDiscoverable, ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if(!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  nsRefPtr<Promise> promise = new Promise(global);

  /**
   * Ensure
   * - mDiscoverable does not equal to aDiscoverable,
   * - adapter is already enabled, and
   * - BluetoothService is available
   */
  BT_ENSURE_TRUE_RESOLVE(mDiscoverable != aDiscoverable,
                         JS::UndefinedHandleValue);
  BT_ENSURE_TRUE_REJECT(mState == BluetoothAdapterState::Enabled,
                        NS_ERROR_DOM_INVALID_STATE_ERR);
  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, NS_ERROR_NOT_AVAILABLE);

  // Wrap property to set and runnable to handle result
  BluetoothNamedValue property(NS_LITERAL_STRING("Discoverable"),
                               BluetoothValue(aDiscoverable));
  nsRefPtr<BluetoothReplyRunnable> result =
    new BluetoothVoidReplyRunnable(nullptr /* DOMRequest */,
                                   promise,
                                   NS_LITERAL_STRING("SetDiscoverable"));
  BT_ENSURE_TRUE_REJECT(
    NS_SUCCEEDED(bs->SetProperty(BluetoothObjectType::TYPE_ADAPTER,
                                 property, result)),
    NS_ERROR_DOM_OPERATION_ERR);

  return promise.forget();
}

already_AddRefed<DOMRequest>
BluetoothAdapter::GetConnectedDevices(uint16_t aServiceUuid, ErrorResult& aRv)
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
  nsresult rv = bs->GetConnectedDevicePropertiesInternal(aServiceUuid, results);
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
BluetoothAdapter::PairUnpair(bool aPair, const nsAString& aDeviceAddress,
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
  nsresult rv;
  if (aPair) {
    rv = bs->CreatePairedDeviceInternal(aDeviceAddress,
                                        kCreatePairedDeviceTimeout,
                                        results);
  } else {
    rv = bs->RemoveDeviceInternal(aDeviceAddress, results);
  }
  if (NS_FAILED(rv)) {
    BT_WARNING("Pair/Unpair failed!");
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<DOMRequest>
BluetoothAdapter::Pair(const nsAString& aDeviceAddress, ErrorResult& aRv)
{
  return PairUnpair(true, aDeviceAddress, aRv);
}

already_AddRefed<DOMRequest>
BluetoothAdapter::Unpair(const nsAString& aDeviceAddress, ErrorResult& aRv)
{
  return PairUnpair(false, aDeviceAddress, aRv);
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
    BT_WARNING("SetPinCode failed!");
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
    BT_WARNING("SetPasskeyInternal failed!");
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
    BT_WARNING("SetPairingConfirmation failed!");
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<Promise>
BluetoothAdapter::EnableDisable(bool aEnable, ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if(!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  nsRefPtr<Promise> promise = new Promise(global);

  // Ensure BluetoothService is available before modifying adapter state
  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, NS_ERROR_NOT_AVAILABLE);

  // Modify adapter state to Enabling/Disabling if adapter is in a valid state
  nsAutoString methodName;
  if (aEnable) {
    // Enable local adapter
    BT_ENSURE_TRUE_REJECT(mState == BluetoothAdapterState::Disabled,
                          NS_ERROR_DOM_INVALID_STATE_ERR);
    methodName.AssignLiteral("Enable");
    mState = BluetoothAdapterState::Enabling;
  } else {
    // Disable local adapter
    BT_ENSURE_TRUE_REJECT(mState == BluetoothAdapterState::Enabled,
                          NS_ERROR_DOM_INVALID_STATE_ERR);
    methodName.AssignLiteral("Disable");
    mState = BluetoothAdapterState::Disabling;
  }

  // Notify applications of adapter state change to Enabling/Disabling
  nsTArray<nsString> types;
  BT_APPEND_ENUM_STRING(types,
                        BluetoothAdapterAttribute,
                        BluetoothAdapterAttribute::State);
  DispatchAttributeEvent(types);

  // Wrap runnable to handle result
  nsRefPtr<BluetoothReplyRunnable> result =
    new BluetoothVoidReplyRunnable(nullptr, /* DOMRequest */
                                   promise,
                                   methodName);

  if(NS_FAILED(bs->EnableDisable(aEnable, result))) {
    mState = aEnable ? BluetoothAdapterState::Disabled
                     : BluetoothAdapterState::Enabled;
    promise->MaybeReject(NS_ERROR_DOM_OPERATION_ERR);
  }

  return promise.forget();
}

already_AddRefed<Promise>
BluetoothAdapter::Enable(ErrorResult& aRv)
{
  return EnableDisable(true, aRv);
}

already_AddRefed<Promise>
BluetoothAdapter::Disable(ErrorResult& aRv)
{
  return EnableDisable(false, aRv);
}

BluetoothAdapterAttribute
BluetoothAdapter::ConvertStringToAdapterAttribute(const nsAString& aString)
{
  using namespace
    mozilla::dom::BluetoothAdapterAttributeValues;

  for (size_t index = 0; index < ArrayLength(strings) - 1; index++) {
    if (aString.LowerCaseEqualsASCII(strings[index].value,
                                     strings[index].length)) {
      return static_cast<BluetoothAdapterAttribute>(index);
    }
  }
  return BluetoothAdapterAttribute::Unknown;
}

bool
BluetoothAdapter::IsAdapterAttributeChanged(BluetoothAdapterAttribute aType,
                                            const BluetoothValue& aValue)
{
  switch(aType) {
    case BluetoothAdapterAttribute::State:
      MOZ_ASSERT(aValue.type() == BluetoothValue::Tbool);
      return aValue.get_bool() ? mState != BluetoothAdapterState::Enabled
                               : mState != BluetoothAdapterState::Disabled;
    case BluetoothAdapterAttribute::Name:
      MOZ_ASSERT(aValue.type() == BluetoothValue::TnsString);
      return !mName.Equals(aValue.get_nsString());
    case BluetoothAdapterAttribute::Address:
      MOZ_ASSERT(aValue.type() == BluetoothValue::TnsString);
      return !mAddress.Equals(aValue.get_nsString());
    case BluetoothAdapterAttribute::Discoverable:
      MOZ_ASSERT(aValue.type() == BluetoothValue::Tbool);
      return mDiscoverable != aValue.get_bool();
    case BluetoothAdapterAttribute::Discovering:
      MOZ_ASSERT(aValue.type() == BluetoothValue::Tbool);
      return mDiscovering != aValue.get_bool();
    default:
      BT_WARNING("Type %d is not handled", uint32_t(aType));
      return false;
  }
}

void
BluetoothAdapter::HandlePropertyChanged(const BluetoothValue& aValue)
{
  MOZ_ASSERT(aValue.type() == BluetoothValue::TArrayOfBluetoothNamedValue);

  const InfallibleTArray<BluetoothNamedValue>& arr =
    aValue.get_ArrayOfBluetoothNamedValue();

  nsTArray<nsString> types;
  for (uint32_t i = 0, propCount = arr.Length(); i < propCount; ++i) {
    BluetoothAdapterAttribute type =
      ConvertStringToAdapterAttribute(arr[i].name());

    // Non-BluetoothAdapterAttribute properties
    if (type == BluetoothAdapterAttribute::Unknown) {
      SetPropertyByValue(arr[i]);
      continue;
    }

    // BluetoothAdapterAttribute properties
    if (IsAdapterAttributeChanged(type, arr[i].value())) {
      SetPropertyByValue(arr[i]);
      BT_APPEND_ENUM_STRING(types, BluetoothAdapterAttribute, type);
    }
  }

  DispatchAttributeEvent(types);
}

void
BluetoothAdapter::DispatchAttributeEvent(const nsTArray<nsString>& aTypes)
{
  NS_ENSURE_TRUE_VOID(aTypes.Length());

  AutoJSAPI jsapi;
  if (!jsapi.Init(GetOwner())) {
    BT_WARNING("Failed to initialise AutoJSAPI!");
    return;
  }
  JSContext* cx = jsapi.cx();
  JS::Rooted<JS::Value> value(cx);
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  NS_ENSURE_TRUE_VOID(global);

  JS::Rooted<JSObject*> scope(cx, global->GetGlobalJSObject());
  NS_ENSURE_TRUE_VOID(scope);

  JSAutoCompartment ac(cx, scope);

  if(!ToJSValue(cx, aTypes, &value)) {
    JS_ClearPendingException(cx);
    return;
  }

  RootedDictionary<BluetoothAttributeEventInit> init(cx);
  init.mAttrs = value;
  nsRefPtr<BluetoothAttributeEvent> event =
    BluetoothAttributeEvent::Constructor(this,
                                         NS_LITERAL_STRING("attributechanged"),
                                         init);
  DispatchTrustedEvent(event);
}

already_AddRefed<DOMRequest>
BluetoothAdapter::Connect(BluetoothDevice& aDevice,
                          const Optional<short unsigned int>& aServiceUuid,
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

  nsAutoString address;
  aDevice.GetAddress(address);
  uint32_t deviceClass = aDevice.Cod()->ToUint32();
  uint16_t serviceUuid = 0;
  if (aServiceUuid.WasPassed()) {
    serviceUuid = aServiceUuid.Value();
  }

  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  bs->Connect(address, deviceClass, serviceUuid, results);

  return request.forget();
}

already_AddRefed<DOMRequest>
BluetoothAdapter::Disconnect(BluetoothDevice& aDevice,
                             const Optional<short unsigned int>& aServiceUuid,
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

  nsAutoString address;
  aDevice.GetAddress(address);
  uint16_t serviceUuid = 0;
  if (aServiceUuid.WasPassed()) {
    serviceUuid = aServiceUuid.Value();
  }

  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  bs->Disconnect(address, serviceUuid, results);

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

  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    // In-process transfer
    bs->SendFile(aDeviceAddress, aBlob, results);
  } else {
    ContentChild *cc = ContentChild::GetSingleton();
    if (!cc) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    BlobChild* actor = cc->GetOrCreateActorForBlob(aBlob);
    if (!actor) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    bs->SendFile(aDeviceAddress, nullptr, actor, results);
  }

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
BluetoothAdapter::AnswerWaitingCall(ErrorResult& aRv)
{
#ifdef MOZ_B2G_RIL
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
  bs->AnswerWaitingCall(results);

  return request.forget();
#else
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return nullptr;
#endif // MOZ_B2G_RIL
}

already_AddRefed<DOMRequest>
BluetoothAdapter::IgnoreWaitingCall(ErrorResult& aRv)
{
#ifdef MOZ_B2G_RIL
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
  bs->IgnoreWaitingCall(results);

  return request.forget();
#else
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return nullptr;
#endif // MOZ_B2G_RIL
}

already_AddRefed<DOMRequest>
BluetoothAdapter::ToggleCalls(ErrorResult& aRv)
{
#ifdef MOZ_B2G_RIL
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
  bs->ToggleCalls(results);

  return request.forget();
#else
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return nullptr;
#endif // MOZ_B2G_RIL
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
BluetoothAdapter::WrapObject(JSContext* aCx)
{
  return BluetoothAdapterBinding::Wrap(aCx, this);
}
