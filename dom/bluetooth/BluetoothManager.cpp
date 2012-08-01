/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"
#include "BluetoothManager.h"
#include "BluetoothCommon.h"
#include "BluetoothAdapter.h"
#include "BluetoothService.h"
#include "BluetoothTypes.h"
#include "BluetoothReplyRunnable.h"

#include "nsContentUtils.h"
#include "mozilla/Preferences.h"
#include "nsIDOMDOMRequest.h"
#include "nsDOMClassInfo.h"
#include "nsDOMEvent.h"
#include "nsThreadUtils.h"
#include "nsXPCOMCIDInternal.h"
#include "mozilla/LazyIdleThread.h"
#include "mozilla/Util.h"

#define DOM_BLUETOOTH_URL_PREF "dom.mozBluetooth.whitelist"

using namespace mozilla;
using mozilla::Preferences;

USING_BLUETOOTH_NAMESPACE

DOMCI_DATA(BluetoothManager, BluetoothManager)

NS_IMPL_CYCLE_COLLECTION_CLASS(BluetoothManager)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(BluetoothManager,
                                                  nsDOMEventTargetHelper)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(enabled)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(BluetoothManager,
                                                nsDOMEventTargetHelper)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(enabled)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(BluetoothManager)
  NS_INTERFACE_MAP_ENTRY(nsIDOMBluetoothManager)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(BluetoothManager)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(BluetoothManager, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(BluetoothManager, nsDOMEventTargetHelper)

class GetAdapterTask : public BluetoothReplyRunnable
{
public:
  GetAdapterTask(BluetoothManager* aManager,
                 nsIDOMDOMRequest* aReq) :
    BluetoothReplyRunnable(aReq),
    mManagerPtr(aManager)
  {
  }

  bool
  ParseSuccessfulReply(jsval* aValue)
  {
    nsCOMPtr<nsIDOMBluetoothAdapter> adapter;
    *aValue = JSVAL_VOID;

    const nsString& path =
      mReply->get_BluetoothReplySuccess().value().get_nsString();
    adapter = BluetoothAdapter::Create(mManagerPtr->GetOwner(),
                                       path);

    nsresult rv;
    nsIScriptContext* sc = mManagerPtr->GetContextForEventHandlers(&rv);
    if (!sc) {
      NS_WARNING("Cannot create script context!");
      SetError(NS_LITERAL_STRING("BluetoothScriptContextError"));
      return false;
    }

    rv = nsContentUtils::WrapNative(sc->GetNativeContext(),
                                    sc->GetNativeGlobal(),
                                    adapter,
                                    aValue);
    bool result = NS_SUCCEEDED(rv) ? true : false;
    if (!result) {
      NS_WARNING("Cannot create native object!");
      SetError(NS_LITERAL_STRING("BluetoothNativeObjectError"));
    }

    return result;
  }

  void
  ReleaseMembers()
  {
    BluetoothReplyRunnable::ReleaseMembers();
    mManagerPtr = nullptr;
  }
  
private:
  nsRefPtr<BluetoothManager> mManagerPtr;
};

class ToggleBtResultTask : public BluetoothReplyRunnable
{
public:
  ToggleBtResultTask(BluetoothManager* aManager,
                     nsIDOMDOMRequest* aReq,
                     bool aEnabled)
    : BluetoothReplyRunnable(aReq),
      mManagerPtr(aManager),
      mEnabled(aEnabled)
  {
  }

  ~ToggleBtResultTask()
  {
  }
  
  bool
  ParseSuccessfulReply(jsval* aValue)
  {
    MOZ_ASSERT(NS_IsMainThread());
    *aValue = JSVAL_VOID;
    mManagerPtr->SetEnabledInternal(mEnabled);
    return true;
  }

  void
  ReleaseMembers()
  {
    BluetoothReplyRunnable::ReleaseMembers();
    // mManagerPtr must be null before returning to prevent the background
    // thread from racing to release it during the destruction of this runnable.
    mManagerPtr = nullptr;
  }
  
private:
  nsRefPtr<BluetoothManager> mManagerPtr;
  bool mEnabled;
};

BluetoothManager::BluetoothManager(nsPIDOMWindow *aWindow) :
  BluetoothPropertyContainer(BluetoothObjectType::TYPE_MANAGER),
  mEnabled(false)
{
  BindToOwner(aWindow);
  mPath.AssignLiteral("/");
}

BluetoothManager::~BluetoothManager()
{
  BluetoothService* bs = BluetoothService::Get();
  // We can be null on shutdown, where this might happen
  if (bs) {
    if (NS_FAILED(bs->UnregisterBluetoothSignalHandler(mPath, this))) {
      NS_WARNING("Failed to unregister object with observer!");
    }
  }
}

void
BluetoothManager::SetPropertyByValue(const BluetoothNamedValue& aValue)
{
#ifdef DEBUG
    const nsString& name = aValue.name();
    nsCString warningMsg;
    warningMsg.AssignLiteral("Not handling manager property: ");
    warningMsg.Append(NS_ConvertUTF16toUTF8(name));
    NS_WARNING(warningMsg.get());
#endif
}

NS_IMETHODIMP
BluetoothManager::SetEnabled(bool aEnabled, nsIDOMDOMRequest** aDomRequest)
{
  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    NS_WARNING("BluetoothService not available!");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDOMRequestService> rs = do_GetService("@mozilla.org/dom/dom-request-service;1");
  if (!rs) {
    NS_WARNING("No DOMRequest Service!");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDOMDOMRequest> request;
  nsresult rv = rs->CreateRequest(GetOwner(), getter_AddRefs(request));
  if (NS_FAILED(rv)) {
    NS_WARNING("Can't create DOM request!");
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<BluetoothReplyRunnable> results = new ToggleBtResultTask(this, request, aEnabled);
  if (aEnabled) {
    if (NS_FAILED(bs->Start(results))) {
      return NS_ERROR_FAILURE;
    }
  }
  else {
    if (NS_FAILED(bs->Stop(results))) {
      return NS_ERROR_FAILURE;
    }
  }

  request.forget(aDomRequest);
  return NS_OK;
}

NS_IMETHODIMP
BluetoothManager::GetEnabled(bool* aEnabled)
{
  *aEnabled = mEnabled;
  return NS_OK;
}

NS_IMETHODIMP
BluetoothManager::GetDefaultAdapter(nsIDOMDOMRequest** aAdapter)
{
  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    NS_WARNING("BluetoothService not available!");
    return NS_ERROR_FAILURE;
  }
  
  nsCOMPtr<nsIDOMRequestService> rs = do_GetService("@mozilla.org/dom/dom-request-service;1");

  if (!rs) {
    NS_WARNING("No DOMRequest Service!");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDOMDOMRequest> request;
  nsresult rv = rs->CreateRequest(GetOwner(), getter_AddRefs(request));
  if (NS_FAILED(rv)) {
    NS_WARNING("Can't create DOMRequest!");
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<BluetoothReplyRunnable> results = new GetAdapterTask(this, request);

  if (NS_FAILED(bs->GetDefaultAdapterPathInternal(results))) {
    return NS_ERROR_FAILURE;
  }
  request.forget(aAdapter);
  return NS_OK;
}

// static
already_AddRefed<BluetoothManager>
BluetoothManager::Create(nsPIDOMWindow* aWindow) {

  nsRefPtr<BluetoothManager> manager = new BluetoothManager(aWindow);
  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    NS_WARNING("BluetoothService not available!");
    return nullptr;
  }
  
  if (NS_FAILED(bs->RegisterBluetoothSignalHandler(NS_LITERAL_STRING("/"), manager))) {
    NS_ERROR("Failed to register object with observer!");
    return nullptr;
  }
  
  return manager.forget();
}

nsresult
NS_NewBluetoothManager(nsPIDOMWindow* aWindow,
                       nsIDOMBluetoothManager** aBluetoothManager)
{
  NS_ASSERTION(aWindow, "Null pointer!");

  bool allowed;
  nsresult rv = nsContentUtils::IsOnPrefWhitelist(aWindow, DOM_BLUETOOTH_URL_PREF, &allowed);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!allowed) {
    *aBluetoothManager = nullptr;
    return NS_OK;
  }

  nsRefPtr<BluetoothManager> bluetoothManager = BluetoothManager::Create(aWindow);
  if (!bluetoothManager) {
    NS_ERROR("Cannot create bluetooth manager!");
    return NS_ERROR_FAILURE;
  }
  bluetoothManager.forget(aBluetoothManager);
  return NS_OK;
}

void
BluetoothManager::Notify(const BluetoothSignal& aData)
{
#ifdef DEBUG
  nsCString warningMsg;
  warningMsg.AssignLiteral("Not handling manager signal: ");
  warningMsg.Append(NS_ConvertUTF16toUTF8(aData.name()));
  NS_WARNING(warningMsg.get());
#endif
}
