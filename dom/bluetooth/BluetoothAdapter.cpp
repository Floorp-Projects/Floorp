/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothAdapter.h"

#include "nsDOMClassInfo.h"
#include "nsDOMEvent.h"
#include "nsThreadUtils.h"
#include "nsXPCOMCIDInternal.h"
#include "mozilla/LazyIdleThread.h"
#include "mozilla/Util.h"
#include <dlfcn.h>

static void
FireEnabled(bool aResult, nsIDOMDOMRequest* aDomRequest)
{
  nsCOMPtr<nsIDOMRequestService> rs = do_GetService("@mozilla.org/dom/dom-request-service;1");

  if (!rs) {
    NS_WARNING("No DOMRequest Service!");
    return;
  }

  mozilla::DebugOnly<nsresult> rv = aResult ?     
                                    rs->FireSuccess(aDomRequest, JSVAL_VOID) :
                                    rs->FireError(aDomRequest, 
                                                  NS_LITERAL_STRING("Bluetooth firmware loading failed"));

  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Bluetooth firmware loading failed");
}

USING_BLUETOOTH_NAMESPACE

static struct BluedroidFunctions {
  bool initialized;
  bool tried_initialization;

  BluedroidFunctions() :
    initialized(false),
    tried_initialization(false)
  {
  }
  
  int (* bt_enable)();
  int (* bt_disable)();
  int (* bt_is_enabled)();
} sBluedroidFunctions;

static bool EnsureBluetoothInit() {
  if (sBluedroidFunctions.tried_initialization)
  {
    return sBluedroidFunctions.initialized;
  }

  sBluedroidFunctions.initialized = false;
  sBluedroidFunctions.tried_initialization = true;
  
  void* handle = dlopen("libbluedroid.so", RTLD_LAZY);

  if(!handle) {
    NS_ERROR("Failed to open libbluedroid.so, bluetooth cannot run");
    return false;
  }

  sBluedroidFunctions.bt_enable = (int (*)())dlsym(handle, "bt_enable");
  if(sBluedroidFunctions.bt_enable == NULL) {
    NS_ERROR("Failed to attach bt_enable function");
    return false;
  }
  sBluedroidFunctions.bt_disable = (int (*)())dlsym(handle, "bt_disable");
  if(sBluedroidFunctions.bt_disable == NULL) {
    NS_ERROR("Failed to attach bt_disable function");
    return false;
  }
  sBluedroidFunctions.bt_is_enabled = (int (*)())dlsym(handle, "bt_is_enabled");
  if(sBluedroidFunctions.bt_is_enabled == NULL) {
    NS_ERROR("Failed to attach bt_is_enabled function");
    return false;
  }
  sBluedroidFunctions.initialized = true;
  return true;
}

class ToggleBtResultTask : public nsRunnable
{
  public:
    ToggleBtResultTask(nsRefPtr<BluetoothAdapter>& adapterPtr, 
                       nsCOMPtr<nsIDOMDOMRequest>& req,
                       bool enabled,
                       bool result)
      : mResult(result),
        mEnabled(enabled)
    {
      MOZ_ASSERT(!NS_IsMainThread());

      mDOMRequest.swap(req);
      mAdapterPtr.swap(adapterPtr);
    }

    NS_IMETHOD Run() 
    {
      MOZ_ASSERT(NS_IsMainThread());

      // Update bt power status to BluetoothAdapter only if loading bluetooth 
      // firmware succeeds.
      if (mResult) {
        mAdapterPtr->SetEnabledInternal(mEnabled);
      }

      FireEnabled(mResult, mDOMRequest);

      //mAdapterPtr must be null before returning to prevent the background 
      //thread from racing to release it during the destruction of this runnable.
      mAdapterPtr = nsnull;
      mDOMRequest = nsnull;

      return NS_OK;
    }

  private:
    nsRefPtr<BluetoothAdapter> mAdapterPtr;
    nsCOMPtr<nsIDOMDOMRequest> mDOMRequest;
    bool mEnabled;
    bool mResult;
};

class ToggleBtTask : public nsRunnable
{
  public:
    ToggleBtTask(bool enabled, nsIDOMDOMRequest* req, BluetoothAdapter* adapterPtr)
      : mEnabled(enabled),
        mDOMRequest(req),
        mAdapterPtr(adapterPtr) 
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
      int isEnabled = sBluedroidFunctions.bt_is_enabled();

      if ((isEnabled == 1 && mEnabled) || (isEnabled == 0 && !mEnabled)) {
        result = true;
      } else if (isEnabled < 0) {
        result = false;
      } else if (mEnabled) {
        result = (sBluedroidFunctions.bt_enable() == 0) ? true : false;
      } else {
        result = (sBluedroidFunctions.bt_disable() == 0) ? true : false;
      }
#else
      result = true;
      NS_WARNING("No bluetooth support in this build configuration, faking a success event instead");
#endif

      // Create a result thread and pass it to Main Thread, 
      nsCOMPtr<nsIRunnable> resultRunnable = new ToggleBtResultTask(mAdapterPtr, mDOMRequest, mEnabled, result);

      if (NS_FAILED(NS_DispatchToMainThread(resultRunnable))) {
        NS_WARNING("Failed to dispatch to main thread!");
      }

      return NS_OK;
    }

  private:
    bool mEnabled;
    nsRefPtr<BluetoothAdapter> mAdapterPtr;
    nsCOMPtr<nsIDOMDOMRequest> mDOMRequest;
};

DOMCI_DATA(BluetoothAdapter, BluetoothAdapter)

NS_IMPL_CYCLE_COLLECTION_CLASS(BluetoothAdapter)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(BluetoothAdapter, 
                                                  nsDOMEventTargetHelper)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(enabled)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(BluetoothAdapter, 
                                                nsDOMEventTargetHelper)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(enabled)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(BluetoothAdapter)
  NS_INTERFACE_MAP_ENTRY(nsIDOMBluetoothAdapter)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(BluetoothAdapter)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(BluetoothAdapter, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(BluetoothAdapter, nsDOMEventTargetHelper)

BluetoothAdapter::BluetoothAdapter(nsPIDOMWindow *aWindow) 
{
  BindToOwner(aWindow);
}

NS_IMETHODIMP
BluetoothAdapter::SetEnabled(bool aEnabled, nsIDOMDOMRequest** aDomRequest)
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
BluetoothAdapter::GetEnabled(bool* aEnabled)
{
  *aEnabled = mEnabled;
  return NS_OK; 
}

