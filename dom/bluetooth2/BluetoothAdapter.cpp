/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothReplyRunnable.h"
#include "BluetoothService.h"
#include "DOMRequest.h"
#include "nsIDocument.h"
#include "nsIPrincipal.h"
#include "nsTArrayHelpers.h"

#include "mozilla/dom/BluetoothAdapter2Binding.h"
#include "mozilla/dom/BluetoothAttributeEvent.h"
#include "mozilla/dom/BluetoothStatusChangedEvent.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/File.h"

#include "mozilla/dom/bluetooth/BluetoothAdapter.h"
#include "mozilla/dom/bluetooth/BluetoothClassOfDevice.h"
#include "mozilla/dom/bluetooth/BluetoothDevice.h"
#include "mozilla/dom/bluetooth/BluetoothDiscoveryHandle.h"
#include "mozilla/dom/bluetooth/BluetoothPairingListener.h"
#include "mozilla/dom/bluetooth/BluetoothTypes.h"

using namespace mozilla;
using namespace mozilla::dom;

USING_BLUETOOTH_NAMESPACE

NS_IMPL_CYCLE_COLLECTION_INHERITED(BluetoothAdapter, DOMEventTargetHelper,
                                   mDevices, mDiscoveryHandleInUse,
                                   mPairingReqs)

// QueryInterface implementation for BluetoothAdapter
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(BluetoothAdapter)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(BluetoothAdapter, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(BluetoothAdapter, DOMEventTargetHelper)

class StartDiscoveryTask MOZ_FINAL : public BluetoothReplyRunnable
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
  GetDevicesTask(BluetoothAdapter* aAdapterPtr, nsIDOMDOMRequest* aReq)
    : BluetoothReplyRunnable(aReq)
    , mAdapterPtr(aAdapterPtr)
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
  , mState(BluetoothAdapterState::Disabled)
  , mDiscoverable(false)
  , mDiscovering(false)
  , mPairingReqs(nullptr)
  , mDiscoveryHandleInUse(nullptr)
{
  MOZ_ASSERT(aWindow);

  // Only allow certified bluetooth application to receive pairing requests
  if (IsBluetoothCertifiedApp()) {
    mPairingReqs = BluetoothPairingListener::Create(aWindow);
  }

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
BluetoothAdapter::GetPairedDeviceProperties(
  const nsTArray<nsString>& aDeviceAddresses)
{
  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  nsRefPtr<BluetoothVoidReplyRunnable> results =
    new BluetoothVoidReplyRunnable(nullptr);

  nsresult rv =
    bs->GetPairedDevicePropertiesInternal(aDeviceAddresses, results);
  if (NS_FAILED(rv)) {
    BT_WARNING("GetPairedDeviceProperties failed");
  }
}

void
BluetoothAdapter::SetPropertyByValue(const BluetoothNamedValue& aValue)
{
  const nsString& name = aValue.name();
  const BluetoothValue& value = aValue.value();
  if (name.EqualsLiteral("State")) {
    mState = value.get_bool() ? BluetoothAdapterState::Enabled
                              : BluetoothAdapterState::Disabled;

    // Clear saved devices when state changes to disabled
    if (mState == BluetoothAdapterState::Disabled) {
      mDevices.Clear();
    }
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
  } else if (name.EqualsLiteral("PairedDevices")) {
    const InfallibleTArray<nsString>& pairedDeviceAddresses
      = value.get_ArrayOfnsString();

    for (uint32_t i = 0; i < pairedDeviceAddresses.Length(); i++) {
      // Check whether or not the address exists in mDevices.
      if (mDevices.Contains(pairedDeviceAddresses[i])) {
        // If the paired device exists in mDevices, it would handle
        // 'PropertyChanged' signal in BluetoothDevice::Notify().
        continue;
      }

      InfallibleTArray<BluetoothNamedValue> props;
      BT_APPEND_NAMED_VALUE(props, "Address", pairedDeviceAddresses[i]);
      BT_APPEND_NAMED_VALUE(props, "Paired", true);

      // Create paired device with 'address' and 'paired' attributes
      nsRefPtr<BluetoothDevice> pairedDevice =
        BluetoothDevice::Create(GetOwner(), BluetoothValue(props));

      // Append to adapter's device array
      mDevices.AppendElement(pairedDevice);
    }

    // Retrieve device properties, result will be handled by device objects.
    GetPairedDeviceProperties(pairedDeviceAddresses);
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
  } else if (aData.name().EqualsLiteral("DeviceFound")) {
    /*
     * DeviceFound signal will be distributed to all existing adapters while
     * doing discovery operations.
     * The signal needs to be handled only if this adapter is holding a valid
     * discovery handle, which means that the discovery operation is triggered
     * by this adapter.
     */
    if (mDiscoveryHandleInUse) {
      HandleDeviceFound(v);
    }
  } else if (aData.name().EqualsLiteral(DEVICE_PAIRED_ID)) {
    HandleDevicePaired(aData.value());
  } else if (aData.name().EqualsLiteral(DEVICE_UNPAIRED_ID)) {
    HandleDeviceUnpaired(aData.value());
  } else if (aData.name().EqualsLiteral(HFP_STATUS_CHANGED_ID) ||
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

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

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

  // Clear unpaired devices before start discovery
  for (int32_t i = mDevices.Length() - 1; i >= 0; i--) {
    if (!mDevices[i]->Paired()) {
      mDevices.RemoveElementAt(i);
    }
  }

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

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

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

already_AddRefed<Promise>
BluetoothAdapter::SetName(const nsAString& aName, ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

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
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

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

void
BluetoothAdapter::GetPairedDevices(nsTArray<nsRefPtr<BluetoothDevice> >& aDevices)
{
  for (uint32_t i = 0; i < mDevices.Length(); ++i) {
    if (mDevices[i]->Paired()) {
      aDevices.AppendElement(mDevices[i]);
    }
  }
}

already_AddRefed<Promise>
BluetoothAdapter::PairUnpair(bool aPair, const nsAString& aDeviceAddress,
                             ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  /**
   * Ensure
   * - device address is not empty,
   * - adapter is already enabled, and
   * - BluetoothService is available.
   */
  BT_ENSURE_TRUE_REJECT(!aDeviceAddress.IsEmpty(),
                        NS_ERROR_DOM_INVALID_STATE_ERR);
  BT_ENSURE_TRUE_REJECT(mState == BluetoothAdapterState::Enabled,
                        NS_ERROR_DOM_INVALID_STATE_ERR);
  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, NS_ERROR_NOT_AVAILABLE);

  nsresult rv;
  if (aPair) {
    nsRefPtr<BluetoothReplyRunnable> result =
      new BluetoothVoidReplyRunnable(nullptr /* DOMRequest */,
                                     promise,
                                     NS_LITERAL_STRING("Pair"));
    rv = bs->CreatePairedDeviceInternal(aDeviceAddress,
                                        kCreatePairedDeviceTimeout,
                                        result);
  } else {
    nsRefPtr<BluetoothReplyRunnable> result =
      new BluetoothVoidReplyRunnable(nullptr /* DOMRequest */,
                                     promise,
                                     NS_LITERAL_STRING("Unpair"));
    rv = bs->RemoveDeviceInternal(aDeviceAddress, result);
  }
  BT_ENSURE_TRUE_REJECT(NS_SUCCEEDED(rv), NS_ERROR_DOM_OPERATION_ERR);

  return promise.forget();
}

already_AddRefed<Promise>
BluetoothAdapter::Pair(const nsAString& aDeviceAddress, ErrorResult& aRv)
{
  return PairUnpair(true, aDeviceAddress, aRv);
}

already_AddRefed<Promise>
BluetoothAdapter::Unpair(const nsAString& aDeviceAddress, ErrorResult& aRv)
{
  return PairUnpair(false, aDeviceAddress, aRv);
}

already_AddRefed<Promise>
BluetoothAdapter::Enable(ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  /**
   * Ensure
   * - adapter is disabled, and
   * - BluetoothService is available.
   */
  BT_ENSURE_TRUE_REJECT(mState == BluetoothAdapterState::Disabled,
                        NS_ERROR_DOM_INVALID_STATE_ERR);
  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, NS_ERROR_NOT_AVAILABLE);

  // Set adapter state "Enabling"
  SetAdapterState(BluetoothAdapterState::Enabling);

  // Wrap runnable to handle result
  nsRefPtr<BluetoothReplyRunnable> result =
    new BluetoothVoidReplyRunnable(nullptr, /* DOMRequest */
                                   promise,
                                   NS_LITERAL_STRING("Enable"));

  if (NS_FAILED(bs->EnableDisable(true, result))) {
    // Restore adapter state and reject promise
    SetAdapterState(BluetoothAdapterState::Disabled);
    promise->MaybeReject(NS_ERROR_DOM_OPERATION_ERR);
  }

  return promise.forget();
}

already_AddRefed<Promise>
BluetoothAdapter::Disable(ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  /**
   * Ensure
   * - adapter is enabled, and
   * - BluetoothService is available.
   */
  BT_ENSURE_TRUE_REJECT(mState == BluetoothAdapterState::Enabled,
                        NS_ERROR_DOM_INVALID_STATE_ERR);
  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, NS_ERROR_NOT_AVAILABLE);

  // Set adapter state "Disabling"
  SetAdapterState(BluetoothAdapterState::Disabling);

  // Wrap runnable to handle result
  nsRefPtr<BluetoothReplyRunnable> result =
    new BluetoothVoidReplyRunnable(nullptr, /* DOMRequest */
                                   promise,
                                   NS_LITERAL_STRING("Disable"));

  if (NS_FAILED(bs->EnableDisable(false, result))) {
    // Restore adapter state and reject promise
    SetAdapterState(BluetoothAdapterState::Enabled);
    promise->MaybeReject(NS_ERROR_DOM_OPERATION_ERR);
  }

  return promise.forget();
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

bool
BluetoothAdapter::IsBluetoothCertifiedApp()
{
  // Retrieve the app status and origin for permission checking
  nsCOMPtr<nsIDocument> doc = GetOwner()->GetExtantDoc();
  NS_ENSURE_TRUE(doc, false);

  uint16_t appStatus = nsIPrincipal::APP_STATUS_NOT_INSTALLED;
  nsAutoCString appOrigin;

  doc->NodePrincipal()->GetAppStatus(&appStatus);
  doc->NodePrincipal()->GetOrigin(getter_Copies(appOrigin));

  return appStatus == nsIPrincipal::APP_STATUS_CERTIFIED &&
         appOrigin.EqualsLiteral(BLUETOOTH_APP_ORIGIN);
}

void
BluetoothAdapter::SetAdapterState(BluetoothAdapterState aState)
{
  if (mState == aState) {
    return;
  }

  mState = aState;

  // Fire BluetoothAttributeEvent for changed adapter state
  nsTArray<nsString> types;
  BT_APPEND_ENUM_STRING(types,
                        BluetoothAdapterAttribute,
                        BluetoothAdapterAttribute::State);
  DispatchAttributeEvent(types);
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
BluetoothAdapter::HandleDeviceFound(const BluetoothValue& aValue)
{
  MOZ_ASSERT(mDiscoveryHandleInUse);
  MOZ_ASSERT(aValue.type() == BluetoothValue::TArrayOfBluetoothNamedValue);

  // Create a temporary discovered BluetoothDevice to check existence
  nsRefPtr<BluetoothDevice> discoveredDevice =
    BluetoothDevice::Create(GetOwner(), aValue);

  size_t index = mDevices.IndexOf(discoveredDevice);
  if (index == mDevices.NoIndex) {
    // New device, append it to adapter's device array
    mDevices.AppendElement(discoveredDevice);
  } else {
    // Existing device, discard temporary discovered device
    discoveredDevice = mDevices[index];
  }

  // Notify application of discovered device via discovery handle
  mDiscoveryHandleInUse->DispatchDeviceEvent(discoveredDevice);
}

void
BluetoothAdapter::DispatchAttributeEvent(const nsTArray<nsString>& aTypes)
{
  NS_ENSURE_TRUE_VOID(aTypes.Length());

  AutoJSAPI jsapi;
  NS_ENSURE_TRUE_VOID(jsapi.Init(GetOwner()));
  JSContext* cx = jsapi.cx();
  JS::Rooted<JS::Value> value(cx);

  if (!ToJSValue(cx, aTypes, &value)) {
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

void
BluetoothAdapter::HandleDevicePaired(const BluetoothValue& aValue)
{
  MOZ_ASSERT(aValue.type() == BluetoothValue::TArrayOfBluetoothNamedValue);

  if (mState != BluetoothAdapterState::Enabled) {
    BT_WARNING("HandleDevicePaired() is called when adapter isn't enabled.");
    return;
  }

  const InfallibleTArray<BluetoothNamedValue>& arr =
    aValue.get_ArrayOfBluetoothNamedValue();

  MOZ_ASSERT(arr.Length() == 2 &&
             arr[0].value().type() == BluetoothValue::TnsString && // Address
             arr[1].value().type() == BluetoothValue::Tbool);      // Paired
  MOZ_ASSERT(!arr[0].value().get_nsString().IsEmpty() &&
             arr[1].value().get_bool());

  nsString deviceAddress = arr[0].value().get_nsString();

  nsRefPtr<BluetoothDevice> pairedDevice = nullptr;

  // Check whether or not the address exists in mDevices.
  size_t index = mDevices.IndexOf(deviceAddress);
  if (index == mDevices.NoIndex) {
    // Create a new device and append it to adapter's device array
    pairedDevice = BluetoothDevice::Create(GetOwner(), aValue);
    mDevices.AppendElement(pairedDevice);
  } else {
    // Use existing device
    pairedDevice = mDevices[index];
  }

  // Notify application of paired device
  BluetoothDeviceEventInit init;
  init.mDevice = pairedDevice;
  DispatchDeviceEvent(NS_LITERAL_STRING("devicepaired"), init);
}

void
BluetoothAdapter::HandleDeviceUnpaired(const BluetoothValue& aValue)
{
  MOZ_ASSERT(aValue.type() == BluetoothValue::TArrayOfBluetoothNamedValue);

  if (mState != BluetoothAdapterState::Enabled) {
    BT_WARNING("HandleDeviceUnpaired() is called when adapter isn't enabled.");
    return;
  }

  const InfallibleTArray<BluetoothNamedValue>& arr =
    aValue.get_ArrayOfBluetoothNamedValue();

  MOZ_ASSERT(arr.Length() == 2 &&
             arr[0].value().type() == BluetoothValue::TnsString && // Address
             arr[1].value().type() == BluetoothValue::Tbool);      // Paired
  MOZ_ASSERT(!arr[0].value().get_nsString().IsEmpty() &&
             !arr[1].value().get_bool());

  nsString deviceAddress = arr[0].value().get_nsString();

  // Remove the device with the same address
  mDevices.RemoveElement(deviceAddress);

  // Notify application of unpaired device
  BluetoothDeviceEventInit init;
  init.mAddress = deviceAddress;
  DispatchDeviceEvent(NS_LITERAL_STRING("deviceunpaired"), init);
}

void
BluetoothAdapter::DispatchDeviceEvent(const nsAString& aType,
                                      const BluetoothDeviceEventInit& aInit)
{
  BT_API2_LOGR("aType (%s)", NS_ConvertUTF16toUTF8(aType).get());

  nsRefPtr<BluetoothDeviceEvent> event =
    BluetoothDeviceEvent::Constructor(this, aType, aInit);
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
                           File& aBlob, ErrorResult& aRv)
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
    bs->SendFile(aDeviceAddress, &aBlob, results);
  } else {
    ContentChild *cc = ContentChild::GetSingleton();
    if (!cc) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    BlobChild* actor = cc->GetOrCreateActorForBlob(&aBlob);
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
