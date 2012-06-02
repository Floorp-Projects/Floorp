/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothManager.h"
#include "BluetoothCommon.h"
#include "BluetoothFirmware.h"
#include "BluetoothAdapter.h"
#include "BluetoothUtils.h"

#include "nsIDocument.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsPIDOMWindow.h"

#include "jsapi.h"
#include "mozilla/Preferences.h"
#include "nsDOMClassInfo.h"
#include "nsDOMEvent.h"
#include "nsThreadUtils.h"
#include "nsXPCOMCIDInternal.h"
#include "mozilla/LazyIdleThread.h"
#include "mozilla/Util.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsContentUtils.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"

#define DOM_BLUETOOTH_URL_PREF "dom.mozBluetooth.whitelist"

using namespace mozilla;
using mozilla::Preferences;

USING_BLUETOOTH_NAMESPACE

static void
FireEnabled(bool aResult, nsIDOMDOMRequest* aDomRequest)
{
  nsCOMPtr<nsIDOMRequestService> rs =
    do_GetService("@mozilla.org/dom/dom-request-service;1");

  if (!rs) {
    NS_WARNING("No DOMRequest Service!");
    return;
  }

  DebugOnly<nsresult> rv =
    aResult ?     
    rs->FireSuccess(aDomRequest, JSVAL_VOID) :
    rs->FireError(aDomRequest, 
                  NS_LITERAL_STRING("Bluetooth firmware loading failed"));

  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Bluetooth firmware loading failed");
}

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

class ToggleBtResultTask : public nsRunnable
{
  public:
    ToggleBtResultTask(nsRefPtr<BluetoothManager>& aManager, 
                       nsCOMPtr<nsIDOMDOMRequest>& aReq,
                       bool aEnabled,
                       bool aResult)
      : mEnabled(aEnabled),
        mResult(aResult)
    {
      MOZ_ASSERT(!NS_IsMainThread());

      mDOMRequest.swap(aReq);
      mManagerPtr.swap(aManager);
    }

    NS_IMETHOD Run() 
    {
      MOZ_ASSERT(NS_IsMainThread());

      // Update bt power status to BluetoothAdapter only if loading bluetooth 
      // firmware succeeds.
      if (mResult) {
        mManagerPtr->SetEnabledInternal(mEnabled);
      }

      FireEnabled(mResult, mDOMRequest);

      //mAdapterPtr must be null before returning to prevent the background 
      //thread from racing to release it during the destruction of this runnable.
      mManagerPtr = NULL;
      mDOMRequest = NULL;

      return NS_OK;
    }

  private:
    nsRefPtr<BluetoothManager> mManagerPtr;
    nsCOMPtr<nsIDOMDOMRequest> mDOMRequest;
    bool mEnabled;
    bool mResult;
};

class ToggleBtTask : public nsRunnable
{
  public:
    ToggleBtTask(bool aEnabled, nsIDOMDOMRequest* aReq,
                 BluetoothManager* aManager)
      : mEnabled(aEnabled),        
        mManagerPtr(aManager),
        mDOMRequest(aReq)
    {
      MOZ_ASSERT(NS_IsMainThread());
    }

    NS_IMETHOD Run() 
    {
      MOZ_ASSERT(!NS_IsMainThread());

      bool result;

#ifdef MOZ_WIDGET_GONK
      // Platform specific check for gonk until object is divided in
      // different implementations per platform. Linux doesn't require
      // bluetooth firmware loading, but code should work otherwise.
      if(!EnsureBluetoothInit()) {
        NS_ERROR("Failed to load bluedroid library.\n");
        return NS_ERROR_FAILURE;
      }

      // return 1 if it's enabled, 0 if it's disabled, and -1 on error
      int isEnabled = IsBluetoothEnabled();

      if ((isEnabled == 1 && mEnabled) || (isEnabled == 0 && !mEnabled)) {
        result = true;
      } else if (isEnabled < 0) {
        result = false;
      } else if (mEnabled) {
        result = (EnableBluetooth() == 0) ? true : false;
      } else {
        result = (DisableBluetooth() == 0) ? true : false;
      }
#else
      result = true;
      NS_WARNING("No bluetooth firmware loading support in this build configuration, faking a success event instead");
#endif

      // Create a result thread and pass it to Main Thread, 
      nsCOMPtr<nsIRunnable> resultRunnable = new ToggleBtResultTask(mManagerPtr, mDOMRequest, mEnabled, result);

      if (NS_FAILED(NS_DispatchToMainThread(resultRunnable))) {
        NS_WARNING("Failed to dispatch to main thread!");
      }

      return NS_OK;
    }

  private:
    bool mEnabled;
    nsRefPtr<BluetoothManager> mManagerPtr;
    nsCOMPtr<nsIDOMDOMRequest> mDOMRequest;
};

BluetoothManager::BluetoothManager(nsPIDOMWindow *aWindow) :
  mEnabled(false),
  mName(nsDependentCString("/"))
{
  BindToOwner(aWindow);
}

BluetoothManager::~BluetoothManager()
{
  if(NS_FAILED(UnregisterBluetoothEventHandler(mName, this))) {
    NS_WARNING("Failed to unregister object with observer!");
  }
}

NS_IMETHODIMP
BluetoothManager::SetEnabled(bool aEnabled, nsIDOMDOMRequest** aDomRequest)
{
  nsCOMPtr<nsIDOMRequestService> rs = do_GetService("@mozilla.org/dom/dom-request-service;1");

  if (!rs) {
    NS_ERROR("No DOMRequest Service!");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDOMDOMRequest> request;
  nsresult rv = rs->CreateRequest(GetOwner(), getter_AddRefs(request));
  NS_ENSURE_SUCCESS(rv, rv);
  
  if (!mToggleBtThread) {
    mToggleBtThread = new LazyIdleThread(15000);
  }

  nsCOMPtr<nsIRunnable> r = new ToggleBtTask(aEnabled, request, this);

  rv = mToggleBtThread->Dispatch(r, NS_DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS(rv, rv);

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
BluetoothManager::GetDefaultAdapter(nsIDOMBluetoothAdapter** aAdapter)
{
  nsCString path;
  nsresult rv = GetDefaultAdapterPathInternal(path);
  if(NS_FAILED(rv)) {
    NS_WARNING("Cannot fetch adapter path!");
    return NS_ERROR_FAILURE;
  }
  nsRefPtr<BluetoothAdapter> adapter = BluetoothAdapter::Create(path);
  adapter.forget(aAdapter);
  return NS_OK;
}

// static
already_AddRefed<BluetoothManager>
BluetoothManager::Create(nsPIDOMWindow* aWindow) {
  nsRefPtr<BluetoothManager> manager = new BluetoothManager(aWindow);
  nsDependentCString name("/");
  if(NS_FAILED(RegisterBluetoothEventHandler(name, manager))) {
    NS_WARNING("Failed to register object with observer!");
    return NULL;
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
    *aBluetoothManager = NULL;
    return NS_OK;
  }

  nsRefPtr<BluetoothManager> bluetoothManager = BluetoothManager::Create(aWindow);

  bluetoothManager.forget(aBluetoothManager);
  return NS_OK;
}

void BluetoothManager::Notify(const BluetoothEvent& aData) {
  printf("Received an manager message!\n");
}
