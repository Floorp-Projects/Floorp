/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothReplyRunnable.h"
#include "BluetoothService.h"
#include "BluetoothUtils.h"
#include "DOMRequest.h"
#include "nsIDocument.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIPrincipal.h"
#include "nsTArrayHelpers.h"

#include "mozilla/dom/BluetoothAdapterBinding.h"
#include "mozilla/dom/BluetoothAttributeEvent.h"
#include "mozilla/dom/BluetoothMapFolderListingEvent.h"
#include "mozilla/dom/BluetoothMapGetMessageEvent.h"
#include "mozilla/dom/BluetoothMapMessagesListingEvent.h"
#include "mozilla/dom/BluetoothMapMessageUpdateEvent.h"
#include "mozilla/dom/BluetoothMapSetMessageStatusEvent.h"
#include "mozilla/dom/BluetoothMapSendMessageEvent.h"
#include "mozilla/dom/BluetoothObexAuthEvent.h"
#include "mozilla/dom/BluetoothPhonebookPullingEvent.h"
#include "mozilla/dom/BluetoothStatusChangedEvent.h"
#include "mozilla/dom/BluetoothVCardListingEvent.h"
#include "mozilla/dom/BluetoothVCardPullingEvent.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/File.h"

#include "mozilla/dom/bluetooth/BluetoothAdapter.h"
#include "mozilla/dom/bluetooth/BluetoothClassOfDevice.h"
#include "mozilla/dom/bluetooth/BluetoothDevice.h"
#include "mozilla/dom/bluetooth/BluetoothDiscoveryHandle.h"
#include "mozilla/dom/bluetooth/BluetoothGattServer.h"
#include "mozilla/dom/bluetooth/BluetoothObexAuthHandle.h"
#include "mozilla/dom/bluetooth/BluetoothPairingListener.h"
#include "mozilla/dom/bluetooth/BluetoothPbapRequestHandle.h"
#include "mozilla/dom/bluetooth/BluetoothTypes.h"

using namespace mozilla;
using namespace mozilla::dom;

USING_BLUETOOTH_NAMESPACE

NS_IMPL_CYCLE_COLLECTION_CLASS(BluetoothAdapter)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(BluetoothAdapter,
                                                DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDevices)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDiscoveryHandleInUse)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPairingReqs)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mLeScanHandleArray)

  /**
   * Unregister the bluetooth signal handler after unlinked.
   *
   * This is needed to avoid ending up with exposing a deleted object to JS or
   * accessing deleted objects while receiving signals from parent process
   * after unlinked. Please see Bug 1138267 for detail informations.
   */
  UnregisterBluetoothSignalHandler(NS_LITERAL_STRING(KEY_ADAPTER), tmp);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(BluetoothAdapter,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDevices)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDiscoveryHandleInUse)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPairingReqs)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mLeScanHandleArray)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

// QueryInterface implementation for BluetoothAdapter
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(BluetoothAdapter)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(BluetoothAdapter, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(BluetoothAdapter, DOMEventTargetHelper)

class StartDiscoveryTask final : public BluetoothReplyRunnable
{
public:
  StartDiscoveryTask(BluetoothAdapter* aAdapter, Promise* aPromise)
    : BluetoothReplyRunnable(nullptr, aPromise)
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
    RefPtr<BluetoothDiscoveryHandle> discoveryHandle =
      BluetoothDiscoveryHandle::Create(mAdapter->GetParentObject());
    if (!ToJSValue(cx, discoveryHandle, aValue)) {
      jsapi.ClearException();
      return false;
    }

    // Set the created discovery handle as the one in use.
    mAdapter->SetDiscoveryHandleInUse(discoveryHandle);
    return true;
  }

  virtual void
  ReleaseMembers() override
  {
    BluetoothReplyRunnable::ReleaseMembers();
    mAdapter = nullptr;
  }

private:
  RefPtr<BluetoothAdapter> mAdapter;
};

class StartLeScanTask final : public BluetoothReplyRunnable
{
public:
  StartLeScanTask(BluetoothAdapter* aAdapter, Promise* aPromise,
                  const nsTArray<BluetoothUuid>& aServiceUuids)
    : BluetoothReplyRunnable(nullptr, aPromise)
    , mAdapter(aAdapter)
    , mServiceUuids(aServiceUuids)
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

    const BluetoothValue& v = mReply->get_BluetoothReplySuccess().value();
    NS_ENSURE_TRUE(v.type() == BluetoothValue::TBluetoothUuid, false);

    /**
     * Create a new discovery handle and wrap it to return. Each
     * discovery handle is one-time-use only.
     */
    RefPtr<BluetoothDiscoveryHandle> discoveryHandle =
      BluetoothDiscoveryHandle::Create(mAdapter->GetParentObject(),
                                       mServiceUuids, v.get_BluetoothUuid());

    if (!ToJSValue(cx, discoveryHandle, aValue)) {
      jsapi.ClearException();
      return false;
    }

    // Append a BluetoothDiscoveryHandle to LeScan handle array.
    mAdapter->AppendLeScanHandle(discoveryHandle);

    return true;
  }

  virtual void
  ReleaseMembers() override
  {
    BluetoothReplyRunnable::ReleaseMembers();
    mAdapter = nullptr;
  }

private:
  RefPtr<BluetoothAdapter> mAdapter;
  nsTArray<BluetoothUuid> mServiceUuids;
};

class StopLeScanTask final : public BluetoothReplyRunnable
{
public:
  StopLeScanTask(BluetoothAdapter* aAdapter,
                 Promise* aPromise,
                 const BluetoothUuid& aScanUuid)
      : BluetoothReplyRunnable(nullptr, aPromise)
      , mAdapter(aAdapter)
      , mScanUuid(aScanUuid)
  {
    MOZ_ASSERT(aPromise);
    MOZ_ASSERT(aAdapter);
    MOZ_ASSERT(!aScanUuid.IsCleared());
  }

protected:
  virtual bool
  ParseSuccessfulReply(JS::MutableHandle<JS::Value> aValue) override
  {
    mAdapter->RemoveLeScanHandle(mScanUuid);
    aValue.setUndefined();
    return true;
  }

  virtual void
  ReleaseMembers() override
  {
    BluetoothReplyRunnable::ReleaseMembers();
    mAdapter = nullptr;
  }

private:
  RefPtr<BluetoothAdapter> mAdapter;
  BluetoothUuid mScanUuid;
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

    nsTArray<RefPtr<BluetoothDevice> > devices;
    for (uint32_t i = 0; i < values.Length(); i++) {
      const BluetoothValue properties = values[i].value();
      if (properties.type() != BluetoothValue::TArrayOfBluetoothNamedValue) {
        BT_WARNING("Not a BluetoothNamedValue array!");
        SetError(NS_LITERAL_STRING("BluetoothReplyTypeError"));
        return false;
      }
      RefPtr<BluetoothDevice> d =
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
  RefPtr<BluetoothAdapter> mAdapterPtr;
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

BluetoothAdapter::BluetoothAdapter(nsPIDOMWindowInner* aWindow,
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

  RegisterBluetoothSignalHandler(NS_LITERAL_STRING(KEY_ADAPTER), this);
}

BluetoothAdapter::~BluetoothAdapter()
{
  Cleanup();
}

void
BluetoothAdapter::DisconnectFromOwner()
{
  DOMEventTargetHelper::DisconnectFromOwner();
  Cleanup();
}

void
BluetoothAdapter::Cleanup()
{
  UnregisterBluetoothSignalHandler(NS_LITERAL_STRING(KEY_ADAPTER), this);

  // Stop ongoing LE scans and clear the LeScan handle array
  if (!mLeScanHandleArray.IsEmpty()) {
    BluetoothService* bs = BluetoothService::Get();
    NS_ENSURE_TRUE_VOID(bs);

    for (size_t i = 0; i < mLeScanHandleArray.Length(); ++i) {
      BluetoothUuid uuid;
      mLeScanHandleArray[i]->GetLeScanUuid(uuid);
      RefPtr<BluetoothVoidReplyRunnable> results =
        new BluetoothVoidReplyRunnable(nullptr);
      bs->StopLeScanInternal(uuid, results);
    }
    mLeScanHandleArray.Clear();
  }
}

BluetoothGattServer*
BluetoothAdapter::GetGattServer()
{
  /* Only expose GATT server if the adapter is enabled. It would be worth
   * noting that the enabling state and the disabling state are just
   * intermediate states, and the adapter would change into the enabled state
   * or the disabled state sooner or later. So we invalidate and nullify the
   * created GATT server object only when the adapter changes to a steady
   * state, i.e., the disabled state.
   */
  if (mState != BluetoothAdapterState::Enabled) {
    return nullptr;
  }

  if (!mGattServer) {
    mGattServer = new BluetoothGattServer(GetOwner());
  }

  return mGattServer;
}

void
BluetoothAdapter::GetPairedDeviceProperties(
  const nsTArray<nsString>& aDeviceAddresses)
{
  nsTArray<BluetoothAddress> deviceAddresses;
  deviceAddresses.SetLength(aDeviceAddresses.Length());

  for (size_t i = 0; i < deviceAddresses.Length(); ++i) {
    auto rv = StringToAddress(aDeviceAddresses[i], deviceAddresses[i]);
    if (NS_FAILED(rv)) {
      BT_WARNING("GetPairedDeviceProperties failed");
      return;
    }
  }

  GetPairedDeviceProperties(deviceAddresses);
}

void
BluetoothAdapter::GetPairedDeviceProperties(
  const nsTArray<BluetoothAddress>& aDeviceAddresses)
{
  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  RefPtr<BluetoothVoidReplyRunnable> results =
    new BluetoothVoidReplyRunnable(nullptr);

  auto rv = bs->GetPairedDevicePropertiesInternal(aDeviceAddresses, results);
  if (NS_FAILED(rv)) {
    BT_WARNING("GetPairedDeviceProperties failed");
    return;
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

    // Clear saved devices and LE scan handles when state changes to disabled
    if (mState == BluetoothAdapterState::Disabled) {
      mDevices.Clear();
      mLeScanHandleArray.Clear();
      if (mGattServer) {
        mGattServer->Invalidate();
        mGattServer = nullptr;
      }
    }
  } else if (name.EqualsLiteral("Name")) {
    RemoteNameToString(value.get_BluetoothRemoteName(), mName);
  } else if (name.EqualsLiteral("Address")) {
    if (value.get_BluetoothAddress().IsCleared()) {
      mAddress.Truncate(); // Reset to empty string
    } else {
      AddressToString(value.get_BluetoothAddress(), mAddress);
    }
  } else if (name.EqualsLiteral("Discoverable")) {
    mDiscoverable = value.get_bool();
  } else if (name.EqualsLiteral("Discovering")) {
    mDiscovering = value.get_bool();
    if (!mDiscovering) {
      // Reset discovery handle in use to nullptr
      SetDiscoveryHandleInUse(nullptr);
    }
  } else if (name.EqualsLiteral("PairedDevices")) {
    const InfallibleTArray<BluetoothAddress>& pairedDeviceAddresses
      = value.get_ArrayOfBluetoothAddress();

    for (uint32_t i = 0; i < pairedDeviceAddresses.Length(); i++) {
      nsString pairedDeviceAddressStr;
      AddressToString(pairedDeviceAddresses[i], pairedDeviceAddressStr);

      if (mDevices.Contains(pairedDeviceAddressStr)) {
        // Existing paired devices handle 'PropertyChanged' signal
        // in BluetoothDevice::Notify()
        continue;
      }

      InfallibleTArray<BluetoothNamedValue> props;
      AppendNamedValue(props, "Address", pairedDeviceAddresses[i]);
      AppendNamedValue(props, "Paired", true);

      // Create paired device and append to adapter's device array
      mDevices.AppendElement(
        BluetoothDevice::Create(GetOwner(), BluetoothValue(props)));
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
BluetoothAdapter::Create(nsPIDOMWindowInner* aWindow, const BluetoothValue& aValue)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);

  RefPtr<BluetoothAdapter> adapter = new BluetoothAdapter(aWindow, aValue);
  return adapter.forget();
}

void
BluetoothAdapter::Notify(const BluetoothSignal& aData)
{
  BT_LOGD("[A] %s", NS_ConvertUTF16toUTF8(aData.name()).get());
  NS_ENSURE_TRUE_VOID(mSignalRegistered);

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
  } else if (aData.name().EqualsLiteral("LeDeviceFound")) {
    if (!mLeScanHandleArray.IsEmpty()) {
      HandleLeDeviceFound(v);
    }
  } else if (aData.name().EqualsLiteral(DEVICE_PAIRED_ID)) {
    HandleDevicePaired(aData.value());
  } else if (aData.name().EqualsLiteral(DEVICE_UNPAIRED_ID)) {
    HandleDeviceUnpaired(aData.value());
  } else if (aData.name().EqualsLiteral(HFP_STATUS_CHANGED_ID) ||
             aData.name().EqualsLiteral(SCO_STATUS_CHANGED_ID) ||
             aData.name().EqualsLiteral(HID_STATUS_CHANGED_ID) ||
             aData.name().EqualsLiteral(A2DP_STATUS_CHANGED_ID)) {
    MOZ_ASSERT(v.type() == BluetoothValue::TArrayOfBluetoothNamedValue);
    const InfallibleTArray<BluetoothNamedValue>& arr =
      v.get_ArrayOfBluetoothNamedValue();

    MOZ_ASSERT(arr.Length() == 2 &&
               arr[0].value().type() == BluetoothValue::TBluetoothAddress &&
               arr[1].value().type() == BluetoothValue::Tbool);
    BluetoothAddress address = arr[0].value().get_BluetoothAddress();
    bool status = arr[1].value().get_bool();

    BluetoothStatusChangedEventInit init;
    init.mBubbles = false;
    init.mCancelable = false;
    AddressToString(address, init.mAddress);
    init.mStatus = status;
    RefPtr<BluetoothStatusChangedEvent> event =
      BluetoothStatusChangedEvent::Constructor(this, aData.name(), init);
    DispatchTrustedEvent(event);
  } else if (aData.name().EqualsLiteral(PAIRING_ABORTED_ID) ||
             aData.name().EqualsLiteral(REQUEST_MEDIA_PLAYSTATUS_ID)) {
    DispatchEmptyEvent(aData.name());
  } else if (aData.name().EqualsLiteral(OBEX_PASSWORD_REQ_ID)) {
    HandleObexPasswordReq(aData.value());
  } else if (aData.name().EqualsLiteral(PULL_PHONEBOOK_REQ_ID)) {
    HandlePullPhonebookReq(aData.value());
  } else if (aData.name().EqualsLiteral(PULL_VCARD_ENTRY_REQ_ID)) {
    HandlePullVCardEntryReq(aData.value());
  } else if (aData.name().EqualsLiteral(PULL_VCARD_LISTING_REQ_ID)) {
    HandlePullVCardListingReq(aData.value());
  } else if (aData.name().EqualsLiteral(MAP_MESSAGES_LISTING_REQ_ID)) {
    HandleMapMessagesListing(aData.value());
  } else if (aData.name().EqualsLiteral(MAP_FOLDER_LISTING_REQ_ID)) {
    HandleMapFolderListing(aData.value());
  } else if (aData.name().EqualsLiteral(MAP_GET_MESSAGE_REQ_ID)) {
    HandleMapGetMessage(aData.value());
  } else if (aData.name().EqualsLiteral(MAP_SET_MESSAGE_STATUS_REQ_ID)) {
    HandleMapSetMessageStatus(aData.value());
  } else if (aData.name().EqualsLiteral(MAP_SEND_MESSAGE_REQ_ID)) {
    HandleMapSendMessage(aData.value());
  } else if (aData.name().EqualsLiteral(MAP_MESSAGE_UPDATE_REQ_ID)) {
    HandleMapMessageUpdate(aData.value());
  } else {
    BT_WARNING("Not handling adapter signal: %s",
               NS_ConvertUTF16toUTF8(aData.name()).get());
  }
}

void
BluetoothAdapter::SetDiscoveryHandleInUse(
  BluetoothDiscoveryHandle* aDiscoveryHandle)
{
  mDiscoveryHandleInUse = aDiscoveryHandle;
}

void
BluetoothAdapter::AppendLeScanHandle(
  BluetoothDiscoveryHandle* aDiscoveryHandle)
{
  mLeScanHandleArray.AppendElement(aDiscoveryHandle);
}

void
BluetoothAdapter::RemoveLeScanHandle(const BluetoothUuid& aScanUuid)
{
  for (size_t i = 0; i < mLeScanHandleArray.Length(); ++i) {
    BluetoothUuid uuid;
    mLeScanHandleArray[i]->GetLeScanUuid(uuid);
    if (aScanUuid == uuid) {
      mLeScanHandleArray.RemoveElementAt(i);
      break;
    }
  }
}

already_AddRefed<Promise>
BluetoothAdapter::StartDiscovery(ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  /**
   * Ensure
   * - adapter is not discovering (note we reject here to ensure
       each resolved promise returns a new BluetoothDiscoveryHandle),
   * - adapter is already enabled, and
   * - BluetoothService is available
   */
  BT_ENSURE_TRUE_REJECT(!mDiscovering, promise, NS_ERROR_DOM_INVALID_STATE_ERR);
  BT_ENSURE_TRUE_REJECT(mState == BluetoothAdapterState::Enabled,
                        promise,
                        NS_ERROR_DOM_INVALID_STATE_ERR);
  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, promise, NS_ERROR_NOT_AVAILABLE);

  // Clear unpaired devices before start discovery
  for (int32_t i = mDevices.Length() - 1; i >= 0; i--) {
    if (!mDevices[i]->Paired()) {
      mDevices.RemoveElementAt(i);
    }
  }

  // Return BluetoothDiscoveryHandle in StartDiscoveryTask
  bs->StartDiscoveryInternal(new StartDiscoveryTask(this, promise));

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

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  /**
   * Ensure
   * - adapter is discovering,
   * - adapter is already enabled, and
   * - BluetoothService is available
   */
  BT_ENSURE_TRUE_RESOLVE(mDiscovering, promise, JS::UndefinedHandleValue);
  BT_ENSURE_TRUE_REJECT(mState == BluetoothAdapterState::Enabled,
                        promise,
                        NS_ERROR_DOM_INVALID_STATE_ERR);
  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, promise, NS_ERROR_NOT_AVAILABLE);

  bs->StopDiscoveryInternal(new BluetoothVoidReplyRunnable(nullptr, promise));

  return promise.forget();
}

already_AddRefed<Promise>
BluetoothAdapter::StartLeScan(const nsTArray<nsString>& aServiceUuids,
                              ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  nsTArray<BluetoothUuid> serviceUuids;
  serviceUuids.SetLength(aServiceUuids.Length());

  for (size_t i = 0; i < serviceUuids.Length(); ++i) {
    BT_ENSURE_TRUE_REJECT(NS_SUCCEEDED(StringToUuid(aServiceUuids[i],
                                                    serviceUuids[i])),
                          promise,
                          NS_ERROR_DOM_OPERATION_ERR);
  }

  BT_ENSURE_TRUE_REJECT(mState == BluetoothAdapterState::Enabled,
                        promise,
                        NS_ERROR_DOM_INVALID_STATE_ERR);

  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, promise, NS_ERROR_NOT_AVAILABLE);

  RefPtr<BluetoothReplyRunnable> result =
    new StartLeScanTask(this, promise, serviceUuids);
  bs->StartLeScanInternal(serviceUuids, result);

  return promise.forget();
}

already_AddRefed<Promise>
BluetoothAdapter::StopLeScan(BluetoothDiscoveryHandle& aDiscoveryHandle,
                             ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  BT_ENSURE_TRUE_REJECT(mState == BluetoothAdapterState::Enabled,
                        promise,
                        NS_ERROR_DOM_INVALID_STATE_ERR);

  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, promise, NS_ERROR_NOT_AVAILABLE);

  // Reject the request if there's no ongoing LE Scan using this handle.
  BT_ENSURE_TRUE_REJECT(mLeScanHandleArray.Contains(&aDiscoveryHandle),
                        promise,
                        NS_ERROR_DOM_BLUETOOTH_DONE);

  BluetoothUuid scanUuid;
  aDiscoveryHandle.GetLeScanUuid(scanUuid);

  RefPtr<BluetoothReplyRunnable> result =
    new StopLeScanTask(this, promise, scanUuid);
  bs->StopLeScanInternal(scanUuid, result);

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

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  /**
   * Ensure
   * - adapter's name does not equal to aName,
   * - adapter is already enabled, and
   * - BluetoothService is available
   */
  BT_ENSURE_TRUE_RESOLVE(!mName.Equals(aName),
                         promise,
                         JS::UndefinedHandleValue);
  BT_ENSURE_TRUE_REJECT(mState == BluetoothAdapterState::Enabled,
                        promise,
                        NS_ERROR_DOM_INVALID_STATE_ERR);
  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, promise, NS_ERROR_NOT_AVAILABLE);

  // Wrap property to set and runnable to handle result
  BluetoothNamedValue property(NS_LITERAL_STRING("Name"),
                               BluetoothValue(
                                 BluetoothRemoteName(
                                   NS_ConvertUTF16toUTF8(aName))));
  BT_ENSURE_SUCCESS_REJECT(
    bs->SetProperty(BluetoothObjectType::TYPE_ADAPTER, property,
                    new BluetoothVoidReplyRunnable(nullptr, promise)),
    promise, NS_ERROR_DOM_OPERATION_ERR);

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

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  /**
   * Ensure
   * - mDiscoverable does not equal to aDiscoverable,
   * - adapter is already enabled, and
   * - BluetoothService is available
   */
  BT_ENSURE_TRUE_RESOLVE(mDiscoverable != aDiscoverable,
                         promise,
                         JS::UndefinedHandleValue);
  BT_ENSURE_TRUE_REJECT(mState == BluetoothAdapterState::Enabled,
                        promise,
                        NS_ERROR_DOM_INVALID_STATE_ERR);
  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, promise, NS_ERROR_NOT_AVAILABLE);

  // Wrap property to set and runnable to handle result
  BluetoothNamedValue property(NS_LITERAL_STRING("Discoverable"),
                               BluetoothValue(aDiscoverable));
  BT_ENSURE_SUCCESS_REJECT(
    bs->SetProperty(BluetoothObjectType::TYPE_ADAPTER, property,
                    new BluetoothVoidReplyRunnable(nullptr, promise)),
    promise, NS_ERROR_DOM_OPERATION_ERR);

  return promise.forget();
}

already_AddRefed<DOMRequest>
BluetoothAdapter::GetConnectedDevices(uint16_t aServiceUuid, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsPIDOMWindowInner> win = GetOwner();
  if (!win) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<DOMRequest> request = new DOMRequest(win);
  RefPtr<BluetoothReplyRunnable> results =
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
BluetoothAdapter::GetPairedDevices(
  nsTArray<RefPtr<BluetoothDevice> >& aDevices)
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

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  /**
   * Ensure
   * - device address is valid,
   * - adapter is already enabled, and
   * - BluetoothService is available.
   */
  BluetoothAddress deviceAddress;
  BT_ENSURE_TRUE_REJECT(NS_SUCCEEDED(StringToAddress(aDeviceAddress,
                                                     deviceAddress)),
                        promise,
                        NS_ERROR_DOM_INVALID_STATE_ERR);
  BT_ENSURE_TRUE_REJECT(mState == BluetoothAdapterState::Enabled,
                        promise,
                        NS_ERROR_DOM_INVALID_STATE_ERR);
  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, promise, NS_ERROR_NOT_AVAILABLE);

  nsresult rv;
  if (aPair) {
    rv = bs->CreatePairedDeviceInternal(
           deviceAddress, kCreatePairedDeviceTimeout,
           new BluetoothVoidReplyRunnable(nullptr, promise));
  } else {
    rv = bs->RemoveDeviceInternal(deviceAddress,
           new BluetoothVoidReplyRunnable(nullptr, promise));
  }
  BT_ENSURE_TRUE_REJECT(NS_SUCCEEDED(rv), promise, NS_ERROR_DOM_OPERATION_ERR);

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

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  /**
   * Ensure
   * - adapter is disabled, and
   * - BluetoothService is available.
   */
  BT_ENSURE_TRUE_REJECT(mState == BluetoothAdapterState::Disabled,
                        promise,
                        NS_ERROR_DOM_INVALID_STATE_ERR);
  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, promise, NS_ERROR_NOT_AVAILABLE);

  // Set adapter state "Enabling"
  SetAdapterState(BluetoothAdapterState::Enabling);

  // Wrap runnable to handle result
  RefPtr<BluetoothReplyRunnable> result =
    new BluetoothVoidReplyRunnable(nullptr, promise);

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

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  /**
   * Ensure
   * - adapter is enabled, and
   * - BluetoothService is available.
   */
  BT_ENSURE_TRUE_REJECT(mState == BluetoothAdapterState::Enabled,
                        promise,
                        NS_ERROR_DOM_INVALID_STATE_ERR);
  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, promise, NS_ERROR_NOT_AVAILABLE);

  // Set adapter state "Disabling"
  SetAdapterState(BluetoothAdapterState::Disabling);

  // Wrap runnable to handle result
  RefPtr<BluetoothReplyRunnable> result =
    new BluetoothVoidReplyRunnable(nullptr, promise);

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
    case BluetoothAdapterAttribute::Name: {
        MOZ_ASSERT(aValue.type() == BluetoothValue::TBluetoothRemoteName);
        nsAutoString name;
        RemoteNameToString(aValue.get_BluetoothRemoteName(), name);
        return !name.Equals(mName);
      }
    case BluetoothAdapterAttribute::Address: {
        MOZ_ASSERT(aValue.type() == BluetoothValue::TBluetoothAddress);
        BluetoothAddress address;
        StringToAddress(mAddress, address);
        return address != aValue.get_BluetoothAddress();
      }
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
  NS_ENSURE_TRUE(GetOwner(), false);

  // Retrieve the app status and origin for permission checking
  nsCOMPtr<nsIDocument> doc = GetOwner()->GetExtantDoc();
  NS_ENSURE_TRUE(doc, false);

  uint16_t appStatus = nsIPrincipal::APP_STATUS_NOT_INSTALLED;
  doc->NodePrincipal()->GetAppStatus(&appStatus);
  if (appStatus != nsIPrincipal::APP_STATUS_CERTIFIED) {
   return false;
  }

  // Get the app origin of Bluetooth app from PrefService.
  nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (!prefs) {
    BT_WARNING("Failed to get preference service");
    return false;
  }

  nsAutoCString prefOrigin;
  nsresult rv = prefs->GetCharPref(PREF_BLUETOOTH_APP_ORIGIN,
                                   getter_Copies(prefOrigin));
  if (NS_FAILED(rv)) {
    BT_WARNING("Failed to get the pref value '" PREF_BLUETOOTH_APP_ORIGIN "'");
    return false;
  }

  nsAutoCString appOrigin;
  doc->NodePrincipal()->GetOriginNoSuffix(appOrigin);

  return appOrigin.Equals(prefOrigin);
}

void
BluetoothAdapter::SetAdapterState(BluetoothAdapterState aState)
{
  if (mState == aState) {
    return;
  }

  mState = aState;

  if (mState == BluetoothAdapterState::Disabled) {
    if (mGattServer) {
      mGattServer->Invalidate();
      mGattServer = nullptr;
    }
  }

  // Fire BluetoothAttributeEvent for changed adapter state
  Sequence<nsString> types;
  BT_APPEND_ENUM_STRING_FALLIBLE(types,
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

  Sequence<nsString> types;
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
      BT_APPEND_ENUM_STRING_FALLIBLE(types, BluetoothAdapterAttribute, type);
    }
  }

  if (types.IsEmpty()) {
    // No adapter attribute changed
    return;
  }

  DispatchAttributeEvent(types);
}

void
BluetoothAdapter::HandleDeviceFound(const BluetoothValue& aValue)
{
  MOZ_ASSERT(mDiscoveryHandleInUse);
  MOZ_ASSERT(aValue.type() == BluetoothValue::TArrayOfBluetoothNamedValue);

  // Create a temporary discovered BluetoothDevice to check existence
  RefPtr<BluetoothDevice> discoveredDevice =
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
BluetoothAdapter::HandleLeDeviceFound(const BluetoothValue& aValue)
{
  MOZ_ASSERT(aValue.type() == BluetoothValue::TArrayOfBluetoothNamedValue);

  const InfallibleTArray<BluetoothNamedValue>& values =
    aValue.get_ArrayOfBluetoothNamedValue();

  int rssi = 0;
  nsTArray<uint8_t> advData;
  for (uint32_t i = 0; i < values.Length(); ++i) {
    nsString name = values[i].name();
    BluetoothValue value = values[i].value();
    if (name.EqualsLiteral("Rssi")) {
      MOZ_ASSERT(value.type() == BluetoothValue::Tint32_t);
      rssi = value.get_int32_t();
    } else if (name.EqualsLiteral("GattAdv")) {
      MOZ_ASSERT(value.type() == BluetoothValue::TArrayOfuint8_t);
      advData = value.get_ArrayOfuint8_t();
    } else {
      BT_WARNING("Receive an unexpected value name '%s'",
                 NS_ConvertUTF16toUTF8(name).get());
    }
  }

  // Create an individual scanned BluetoothDevice for each LeDeviceEvent even
  // the device exists in adapter's devices array
  RefPtr<BluetoothDevice> scannedDevice =
    BluetoothDevice::Create(GetOwner(), aValue);

  // Notify application of scanned devices via discovery handle
  for (uint32_t i = 0; i < mLeScanHandleArray.Length(); ++i) {
    mLeScanHandleArray[i]->DispatchLeDeviceEvent(scannedDevice, rssi, advData);
  }
}

void
BluetoothAdapter::HandleDevicePaired(const BluetoothValue& aValue)
{
  if (NS_WARN_IF(mState != BluetoothAdapterState::Enabled)) {
    return;
  }

  MOZ_ASSERT(aValue.type() == BluetoothValue::TArrayOfBluetoothNamedValue);

  const InfallibleTArray<BluetoothNamedValue>& arr =
    aValue.get_ArrayOfBluetoothNamedValue();

  MOZ_ASSERT(arr.Length() == 3 &&
             arr[0].value().type() == BluetoothValue::TBluetoothAddress && // Address
             arr[1].value().type() == BluetoothValue::TBluetoothRemoteName && // Name
             arr[2].value().type() == BluetoothValue::Tbool);      // Paired
  MOZ_ASSERT(!arr[0].value().get_BluetoothAddress().IsCleared() &&
             arr[2].value().get_bool());

  // Append the paired device if it doesn't exist in adapter's devices array
  nsString addressStr;
  AddressToString(arr[0].value().get_BluetoothAddress(), addressStr);
  size_t index = mDevices.IndexOf(addressStr);
  if (index == mDevices.NoIndex) {
    index = mDevices.Length(); // the new device's index
    mDevices.AppendElement(
      BluetoothDevice::Create(GetOwner(), aValue));
  }

  // Notify application of paired device
  BluetoothDeviceEventInit init;
  init.mDevice = mDevices[index];
  DispatchDeviceEvent(NS_LITERAL_STRING(DEVICE_PAIRED_ID), init);
}

void
BluetoothAdapter::HandleDeviceUnpaired(const BluetoothValue& aValue)
{
  if (NS_WARN_IF(mState != BluetoothAdapterState::Enabled)) {
    return;
  }

  MOZ_ASSERT(aValue.type() == BluetoothValue::TArrayOfBluetoothNamedValue);

  const InfallibleTArray<BluetoothNamedValue>& arr =
    aValue.get_ArrayOfBluetoothNamedValue();

  MOZ_ASSERT(arr.Length() == 2 &&
             arr[0].value().type() == BluetoothValue::TBluetoothAddress && // Address
             arr[1].value().type() == BluetoothValue::Tbool);              // Paired
  MOZ_ASSERT(!arr[0].value().get_BluetoothAddress().IsCleared() &&
             !arr[1].value().get_bool());

  // Remove the device with the same address
  nsString deviceAddress;
  AddressToString(arr[0].value().get_BluetoothAddress(), deviceAddress);
  mDevices.RemoveElement(deviceAddress);

  // Notify application of unpaired device
  BluetoothDeviceEventInit init;
  init.mAddress = deviceAddress;
  DispatchDeviceEvent(NS_LITERAL_STRING(DEVICE_UNPAIRED_ID), init);
}

void
BluetoothAdapter::HandleObexPasswordReq(const BluetoothValue& aValue)
{
  MOZ_ASSERT(aValue.type() == BluetoothValue::TArrayOfBluetoothNamedValue);
  MOZ_ASSERT(aValue.get_ArrayOfBluetoothNamedValue().Length() <= 1);

  BluetoothObexAuthEventInit init;
  init.mHandle = BluetoothObexAuthHandle::Create(GetOwner());

  // TODO: Retrieve optional userId from aValue and assign into event

  RefPtr<BluetoothObexAuthEvent> event =
    BluetoothObexAuthEvent::Constructor(this,
      NS_LITERAL_STRING(OBEX_PASSWORD_REQ_ID), init);
  DispatchTrustedEvent(event);
}

void
BluetoothAdapter::HandlePullPhonebookReq(const BluetoothValue& aValue)
{
  MOZ_ASSERT(aValue.type() == BluetoothValue::TArrayOfBluetoothNamedValue);
  const InfallibleTArray<BluetoothNamedValue>& arr =
    aValue.get_ArrayOfBluetoothNamedValue();

  MOZ_ASSERT(arr.Length() >= 1 &&
             arr[0].value().type() == BluetoothValue::TnsString);

  BluetoothPhonebookPullingEventInit init;

  for (uint32_t i = 0, propCount = arr.Length(); i < propCount; ++i) {
    const nsString& name = arr[i].name();
    const BluetoothValue& value = arr[i].value();
    if (name.EqualsLiteral("name")) {
      init.mName = value.get_nsString();
    } else if (name.EqualsLiteral("format")) {
      init.mFormat = value.get_bool() ? vCardVersion::VCard30
                                      : vCardVersion::VCard21;
    } else if (name.EqualsLiteral("propSelector")) {
      init.mPropSelector = getVCardProperties(value);
    } else if (name.EqualsLiteral("maxListCount")) {
      init.mMaxListCount = value.get_uint32_t();
    } else if (name.EqualsLiteral("listStartOffset")) {
      init.mListStartOffset = value.get_uint32_t();
    } else if (name.EqualsLiteral("vCardSelector_AND")) {
      init.mVcardSelector = getVCardProperties(value);
      init.mVcardSelectorOperator = vCardSelectorOp::AND;
    } else if (name.EqualsLiteral("vCardSelector_OR")) {
      init.mVcardSelector = getVCardProperties(value);
      init.mVcardSelectorOperator = vCardSelectorOp::OR;
    }
  }

  init.mHandle = BluetoothPbapRequestHandle::Create(GetOwner());

  RefPtr<BluetoothPhonebookPullingEvent> event =
    BluetoothPhonebookPullingEvent::Constructor(this,
      NS_LITERAL_STRING(PULL_PHONEBOOK_REQ_ID), init);
  DispatchTrustedEvent(event);
}

void
BluetoothAdapter::HandlePullVCardEntryReq(const BluetoothValue& aValue)
{
  MOZ_ASSERT(aValue.type() == BluetoothValue::TArrayOfBluetoothNamedValue);
  const InfallibleTArray<BluetoothNamedValue>& arr =
    aValue.get_ArrayOfBluetoothNamedValue();

  MOZ_ASSERT(arr.Length() >= 1 &&
             arr[0].value().type() == BluetoothValue::TnsString);

  BluetoothVCardPullingEventInit init;
  Sequence<vCardProperties> propSelector;

  for (uint32_t i = 0, propCount = arr.Length(); i < propCount; ++i) {
    const nsString& name = arr[i].name();
    const BluetoothValue& value = arr[i].value();
    if (name.EqualsLiteral("name")) {
      init.mName = value.get_nsString();
    } else if (name.EqualsLiteral("format")) {
      init.mFormat = value.get_bool() ? vCardVersion::VCard30
                                      : vCardVersion::VCard21;
    } else if (name.EqualsLiteral("propSelector")) {
      init.mPropSelector = getVCardProperties(value);
    }
  }

  init.mHandle = BluetoothPbapRequestHandle::Create(GetOwner());

  RefPtr<BluetoothVCardPullingEvent> event =
    BluetoothVCardPullingEvent::Constructor(this,
      NS_LITERAL_STRING(PULL_VCARD_ENTRY_REQ_ID), init);
  DispatchTrustedEvent(event);
}

void
BluetoothAdapter::HandlePullVCardListingReq(const BluetoothValue& aValue)
{
  MOZ_ASSERT(aValue.type() == BluetoothValue::TArrayOfBluetoothNamedValue);
  const InfallibleTArray<BluetoothNamedValue>& arr =
    aValue.get_ArrayOfBluetoothNamedValue();

  MOZ_ASSERT(arr.Length() >= 1 &&
             arr[0].value().type() == BluetoothValue::TnsString);

  BluetoothVCardListingEventInit init;

  for (uint32_t i = 0, propCount = arr.Length(); i < propCount; ++i) {
    const nsString& name = arr[i].name();
    const BluetoothValue& value = arr[i].value();
    if (name.EqualsLiteral("name")) {
      init.mName = value.get_nsString();
    } else if (name.EqualsLiteral("order")) {
      init.mOrder = static_cast<vCardOrderType>(value.get_uint32_t());
    } else if (name.EqualsLiteral("searchKey")) {
      init.mSearchKey = static_cast<vCardSearchKeyType>(value.get_uint32_t());
    } else if (name.EqualsLiteral("searchText")) {
      init.mSearchValue = NS_ConvertUTF8toUTF16(value.get_nsCString());
    } else if (name.EqualsLiteral("maxListCount")) {
      init.mMaxListCount = value.get_uint32_t();
    } else if (name.EqualsLiteral("listStartOffset")) {
      init.mListStartOffset = value.get_uint32_t();
    } else if (name.EqualsLiteral("vCardSelector_AND")) {
      init.mVcardSelector = getVCardProperties(value);
      init.mVcardSelectorOperator = vCardSelectorOp::AND;
    } else if (name.EqualsLiteral("vCardSelector_OR")) {
      init.mVcardSelector = getVCardProperties(value);
      init.mVcardSelectorOperator = vCardSelectorOp::OR;
    }
  }

  init.mHandle = BluetoothPbapRequestHandle::Create(GetOwner());

  RefPtr<BluetoothVCardListingEvent> event =
    BluetoothVCardListingEvent::Constructor(this,
      NS_LITERAL_STRING(PULL_VCARD_LISTING_REQ_ID), init);
  DispatchTrustedEvent(event);
}

Sequence<vCardProperties>
BluetoothAdapter::getVCardProperties(const BluetoothValue &aValue)
{
  MOZ_ASSERT(aValue.type() == BluetoothValue::TArrayOfuint32_t);

  Sequence<vCardProperties> propSelector;

  const InfallibleTArray<uint32_t>& propSelectorArr =
    aValue.get_ArrayOfuint32_t();
  for (uint32_t i = 0; i < propSelectorArr.Length(); ++i) {
    propSelector.AppendElement(
      static_cast<vCardProperties>(propSelectorArr[i]), mozilla::fallible);
  }

  return propSelector;
}

Sequence<ParameterMask>
BluetoothAdapter::GetParameterMask(const BluetoothValue &aValue)
{
  MOZ_ASSERT(aValue.type() == BluetoothValue::TArrayOfuint32_t);

  Sequence<ParameterMask> parameterMask;

  const InfallibleTArray<uint32_t>& parameterMaskArr =
    aValue.get_ArrayOfuint32_t();
  for (uint32_t i = 0; i < parameterMaskArr.Length(); ++i) {
    parameterMask.AppendElement(
      static_cast<ParameterMask>(parameterMaskArr[i]), mozilla::fallible);
  }

  return parameterMask;
}

void
BluetoothAdapter::HandleMapFolderListing(const BluetoothValue& aValue)
{
  MOZ_ASSERT(aValue.type() == BluetoothValue::TArrayOfBluetoothNamedValue);

  const InfallibleTArray<BluetoothNamedValue>& arr =
    aValue.get_ArrayOfBluetoothNamedValue();

  MOZ_ASSERT(arr.Length() >= 1 &&
             arr[0].value().type() == BluetoothValue::Tuint32_t);

  BluetoothMapFolderListingEventInit init;

  for (uint32_t i = 0, propCount = arr.Length(); i < propCount; ++i) {
    const nsString& name = arr[i].name();
    const BluetoothValue& value = arr[i].value();
    if (name.EqualsLiteral("maxListCount")) {
      init.mMaxListCount = value.get_uint32_t();
    } else if (name.EqualsLiteral("startOffset")) {
      init.mListStartOffset = value.get_uint32_t();
    }
  }

  init.mHandle = BluetoothMapRequestHandle::Create(GetOwner());

  RefPtr<BluetoothMapFolderListingEvent> event =
    BluetoothMapFolderListingEvent::Constructor(this,
      NS_LITERAL_STRING(MAP_FOLDER_LISTING_REQ_ID), init);
  DispatchTrustedEvent(event);
}

void
BluetoothAdapter::HandleMapMessagesListing(const BluetoothValue& aValue)
{
  MOZ_ASSERT(aValue.type() == BluetoothValue::TArrayOfBluetoothNamedValue);

  const InfallibleTArray<BluetoothNamedValue>& arr =
    aValue.get_ArrayOfBluetoothNamedValue();

  MOZ_ASSERT(arr.Length() >= 1 &&
    arr[0].value().type() == BluetoothValue::Tuint32_t);

  BluetoothMapMessagesListingEventInit init;

  for (uint32_t i = 0, propCount = arr.Length(); i < propCount; ++i) {
    const nsString& name = arr[i].name();
    const BluetoothValue& value = arr[i].value();
    if (name.EqualsLiteral("maxListCount")) {
      init.mMaxListCount = value.get_uint32_t();
    } else if (name.EqualsLiteral("startOffset")) {
      init.mListStartOffset = value.get_uint32_t();
    } else if (name.EqualsLiteral("subLength")) {
      init.mSubjectLength = value.get_uint32_t();
    } else if (name.EqualsLiteral("parameterMask")) {
      init.mParameterMask = GetParameterMask(value);
    } else if (name.EqualsLiteral("filterMessageType")) {
      init.mFilterMessageType = static_cast<MessageType>(value.get_uint32_t());
    } else if (name.EqualsLiteral("filterPeriodBegin")) {
      init.mFilterPeriodBegin = value.get_nsString();
    } else if (name.EqualsLiteral("filterPeriodEnd")) {
      init.mFilterPeriodEnd = value.get_nsString();
    } else if (name.EqualsLiteral("filterReadStatus")) {
      init.mFilterReadStatus = static_cast<ReadStatus>(value.get_uint32_t());
    } else if (name.EqualsLiteral("filterRecipient")) {
      init.mFilterRecipient = value.get_nsString();
    } else if (name.EqualsLiteral("filterOriginator")) {
      init.mFilterOriginator = value.get_nsString();
    } else if (name.EqualsLiteral("filterPriority")) {
      init.mFilterPriority = static_cast<Priority>(value.get_uint32_t());
    }
  }

  init.mHandle = BluetoothMapRequestHandle::Create(GetOwner());

  RefPtr<BluetoothMapMessagesListingEvent> event =
    BluetoothMapMessagesListingEvent::Constructor(this,
      NS_LITERAL_STRING(MAP_MESSAGES_LISTING_REQ_ID), init);
  DispatchTrustedEvent(event);
}

void
BluetoothAdapter::HandleMapGetMessage(const BluetoothValue& aValue)
{
  MOZ_ASSERT(aValue.type() == BluetoothValue::TArrayOfBluetoothNamedValue);

  const InfallibleTArray<BluetoothNamedValue>& arr =
    aValue.get_ArrayOfBluetoothNamedValue();

  MOZ_ASSERT(arr.Length() >= 1 &&
    arr[0].value().type() == BluetoothValue::Tbool);

  BluetoothMapGetMessageEventInit init;

  for (uint32_t i = 0, propCount = arr.Length(); i < propCount; ++i) {
    const nsString& name = arr[i].name();
    const BluetoothValue& value = arr[i].value();
    if (name.EqualsLiteral("hasAttachment")) {
      init.mHasAttachment = value.get_bool();
    } else if (name.EqualsLiteral("charset")) {
      if (value.get_uint32_t() == 0) {
        init.mCharset = FilterCharset::Native;
      } else {
        init.mCharset = FilterCharset::Utf_8;
      }
    }
  }

  init.mHandle = BluetoothMapRequestHandle::Create(GetOwner());

  RefPtr<BluetoothMapGetMessageEvent> event =
    BluetoothMapGetMessageEvent::Constructor(this,
      NS_LITERAL_STRING(MAP_GET_MESSAGE_REQ_ID), init);
  DispatchTrustedEvent(event);
}

void
BluetoothAdapter::HandleMapSetMessageStatus(const BluetoothValue& aValue)
{
  MOZ_ASSERT(aValue.type() == BluetoothValue::TArrayOfBluetoothNamedValue);

  const InfallibleTArray<BluetoothNamedValue>& arr =
    aValue.get_ArrayOfBluetoothNamedValue();

  MOZ_ASSERT(arr.Length() >= 1 &&
    arr[0].value().type() == BluetoothValue::Tuint32_t);

  BluetoothMapSetMessageStatusEventInit init;

  for (uint32_t i = 0, propCount = arr.Length(); i < propCount; ++i) {
    const nsString& name = arr[i].name();
    const BluetoothValue& value = arr[i].value();
    if (name.EqualsLiteral("handleId")) {
      init.mHandleId = value.get_uint32_t();
    } else if (name.EqualsLiteral("statusIndicator")) {
      if (value.get_uint32_t()) {
        init.mStatusIndicator = StatusIndicators::Deletedstatus;
      } else {
        init.mStatusIndicator = StatusIndicators::Readstatus;
      }
    } else if (name.EqualsLiteral("statusValue")) {
      init.mStatusValue = static_cast<bool>(value.get_uint32_t());
    }
  }

  init.mHandle = BluetoothMapRequestHandle::Create(GetOwner());

  RefPtr<BluetoothMapSetMessageStatusEvent> event =
    BluetoothMapSetMessageStatusEvent::Constructor(this,
      NS_LITERAL_STRING(MAP_SET_MESSAGE_STATUS_REQ_ID), init);
  DispatchTrustedEvent(event);
}

void
BluetoothAdapter::HandleMapSendMessage(const BluetoothValue& aValue)
{
  MOZ_ASSERT(aValue.type() == BluetoothValue::TArrayOfBluetoothNamedValue);

  const InfallibleTArray<BluetoothNamedValue>& arr =
    aValue.get_ArrayOfBluetoothNamedValue();

  MOZ_ASSERT(arr.Length() >= 1 &&
    arr[0].value().type() == BluetoothValue::TnsString);

  BluetoothMapSendMessageEventInit init;

  for (uint32_t i = 0, propCount = arr.Length(); i < propCount; ++i) {
    const nsString& name = arr[i].name();
    const BluetoothValue& value = arr[i].value();
    if (name.EqualsLiteral("recipient")) {
      init.mRecipient = NS_ConvertUTF8toUTF16(value.get_nsCString());
    } else if (name.EqualsLiteral("messageBody")) {
      init.mMessageBody = NS_ConvertUTF8toUTF16(value.get_nsCString());
    } else if (name.EqualsLiteral("retry")) {
      init.mRetry = value.get_uint32_t();
    }
  }

  init.mHandle = BluetoothMapRequestHandle::Create(GetOwner());

  RefPtr<BluetoothMapSendMessageEvent> event =
    BluetoothMapSendMessageEvent::Constructor(this,
      NS_LITERAL_STRING(MAP_SEND_MESSAGE_REQ_ID), init);
  DispatchTrustedEvent(event);
}

void
BluetoothAdapter::HandleMapMessageUpdate(const BluetoothValue& aValue)
{
  MOZ_ASSERT(aValue.type() == BluetoothValue::TArrayOfBluetoothNamedValue);

  const InfallibleTArray<BluetoothNamedValue>& arr =
    aValue.get_ArrayOfBluetoothNamedValue();

  MOZ_ASSERT(arr.Length() >= 1 &&
    arr[0].value().type() == BluetoothValue::TnsString);

  BluetoothMapMessageUpdateEventInit init;

  for (uint32_t i = 0, propCount = arr.Length(); i < propCount; ++i) {
    const nsString& name = arr[i].name();
    const BluetoothValue& value = arr[i].value();
    if (name.EqualsLiteral("instanceId")) {
      init.mInstanceId = value.get_uint32_t();
    }
  }

  init.mHandle = BluetoothMapRequestHandle::Create(GetOwner());

  RefPtr<BluetoothMapMessageUpdateEvent> event =
    BluetoothMapMessageUpdateEvent::Constructor(this,
      NS_LITERAL_STRING(MAP_MESSAGE_UPDATE_REQ_ID), init);
  DispatchTrustedEvent(event);
}


void
BluetoothAdapter::DispatchAttributeEvent(const Sequence<nsString>& aTypes)
{
  MOZ_ASSERT(!aTypes.IsEmpty());

  BluetoothAttributeEventInit init;
  init.mAttrs = aTypes;

  RefPtr<BluetoothAttributeEvent> event =
    BluetoothAttributeEvent::Constructor(
      this, NS_LITERAL_STRING(ATTRIBUTE_CHANGED_ID), init);

  DispatchTrustedEvent(event);
}

void
BluetoothAdapter::DispatchDeviceEvent(const nsAString& aType,
                                      const BluetoothDeviceEventInit& aInit)
{
  RefPtr<BluetoothDeviceEvent> event =
    BluetoothDeviceEvent::Constructor(this, aType, aInit);
  DispatchTrustedEvent(event);
}

void
BluetoothAdapter::DispatchEmptyEvent(const nsAString& aType)
{
  RefPtr<Event> event = NS_NewDOMEvent(this, nullptr, nullptr);

  event->InitEvent(aType, false, false);

  DispatchTrustedEvent(event);
}

already_AddRefed<DOMRequest>
BluetoothAdapter::Connect(BluetoothDevice& aDevice,
                          const Optional<short unsigned int>& aServiceUuid,
                          ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindowInner> win = GetOwner();
  if (!win) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<DOMRequest> request = new DOMRequest(win);
  RefPtr<BluetoothVoidReplyRunnable> results =
    new BluetoothVoidReplyRunnable(request);

  nsAutoString address;
  aDevice.GetAddress(address);
  BluetoothAddress deviceAddress;
  if (NS_FAILED(StringToAddress(address, deviceAddress))) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

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
  bs->Connect(deviceAddress, deviceClass, serviceUuid, results);

  return request.forget();
}

already_AddRefed<DOMRequest>
BluetoothAdapter::Disconnect(BluetoothDevice& aDevice,
                             const Optional<short unsigned int>& aServiceUuid,
                             ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindowInner> win = GetOwner();
  if (!win) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<DOMRequest> request = new DOMRequest(win);
  RefPtr<BluetoothVoidReplyRunnable> results =
    new BluetoothVoidReplyRunnable(request);

  nsAutoString address;
  aDevice.GetAddress(address);
  BluetoothAddress deviceAddress;
  if (NS_FAILED(StringToAddress(address, deviceAddress))) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  uint16_t serviceUuid = 0;
  if (aServiceUuid.WasPassed()) {
    serviceUuid = aServiceUuid.Value();
  }

  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  bs->Disconnect(deviceAddress, serviceUuid, results);

  return request.forget();
}

already_AddRefed<DOMRequest>
BluetoothAdapter::SendFile(const nsAString& aDeviceAddress,
                           Blob& aBlob, ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindowInner> win = GetOwner();
  if (!win) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<DOMRequest> request = new DOMRequest(win);
  RefPtr<BluetoothVoidReplyRunnable> results =
    new BluetoothVoidReplyRunnable(request);

  BluetoothAddress deviceAddress;
  auto rv = StringToAddress(aDeviceAddress, deviceAddress);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  if (XRE_IsParentProcess()) {
    // In-process transfer
    bs->SendFile(deviceAddress, &aBlob, results);
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

    bs->SendFile(deviceAddress, nullptr, actor, results);
  }

  return request.forget();
}

already_AddRefed<DOMRequest>
BluetoothAdapter::StopSendingFile(
  const nsAString& aDeviceAddress, ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindowInner> win = GetOwner();
  if (!win) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<DOMRequest> request = new DOMRequest(win);
  RefPtr<BluetoothVoidReplyRunnable> results =
    new BluetoothVoidReplyRunnable(request);

  BluetoothAddress deviceAddress;
  auto rv = StringToAddress(aDeviceAddress, deviceAddress);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  bs->StopSendingFile(deviceAddress, results);

  return request.forget();
}

already_AddRefed<DOMRequest>
BluetoothAdapter::ConfirmReceivingFile(const nsAString& aDeviceAddress,
                                       bool aConfirmation, ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindowInner> win = GetOwner();
  if (!win) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<DOMRequest> request = new DOMRequest(win);
  RefPtr<BluetoothVoidReplyRunnable> results =
    new BluetoothVoidReplyRunnable(request);

  BluetoothAddress deviceAddress;
  auto rv = StringToAddress(aDeviceAddress, deviceAddress);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  bs->ConfirmReceivingFile(deviceAddress, aConfirmation, results);

  return request.forget();
}

already_AddRefed<DOMRequest>
BluetoothAdapter::ConnectSco(ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindowInner> win = GetOwner();
  if (!win) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<DOMRequest> request = new DOMRequest(win);
  RefPtr<BluetoothVoidReplyRunnable> results =
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
  nsCOMPtr<nsPIDOMWindowInner> win = GetOwner();
  if (!win) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<DOMRequest> request = new DOMRequest(win);
  RefPtr<BluetoothVoidReplyRunnable> results =
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
  nsCOMPtr<nsPIDOMWindowInner> win = GetOwner();
  if (!win) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<DOMRequest> request = new DOMRequest(win);
  RefPtr<BluetoothReplyRunnable> results =
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
  nsCOMPtr<nsPIDOMWindowInner> win = GetOwner();
  if (!win) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<DOMRequest> request = new DOMRequest(win);
  RefPtr<BluetoothVoidReplyRunnable> results =
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
  nsCOMPtr<nsPIDOMWindowInner> win = GetOwner();
  if (!win) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<DOMRequest> request = new DOMRequest(win);
  RefPtr<BluetoothVoidReplyRunnable> results =
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
  nsCOMPtr<nsPIDOMWindowInner> win = GetOwner();
  if (!win) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<DOMRequest> request = new DOMRequest(win);
  RefPtr<BluetoothVoidReplyRunnable> results =
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
BluetoothAdapter::SendMediaMetaData(
  const MediaMetaData& aMediaMetaData, ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindowInner> win = GetOwner();
  if (!win) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<DOMRequest> request = new DOMRequest(win);
  RefPtr<BluetoothReplyRunnable> results =
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
BluetoothAdapter::SendMediaPlayStatus(
  const MediaPlayStatus& aMediaPlayStatus, ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindowInner> win = GetOwner();
  if (!win) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  ControlPlayStatus playStatus;
  auto rv = StringToControlPlayStatus(aMediaPlayStatus.mPlayStatus, playStatus);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  RefPtr<DOMRequest> request = new DOMRequest(win);
  RefPtr<BluetoothReplyRunnable> results =
    new BluetoothVoidReplyRunnable(request);

  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  bs->SendPlayStatus(aMediaPlayStatus.mDuration,
                     aMediaPlayStatus.mPosition,
                     playStatus,
                     results);

  return request.forget();
}

JSObject*
BluetoothAdapter::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return BluetoothAdapterBinding::Wrap(aCx, this, aGivenProto);
}
