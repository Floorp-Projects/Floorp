/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMClassInfo.h"
#include "nsDOMEvent.h"
#include "nsThreadUtils.h"
#include "nsXPCOMCIDInternal.h"
#include "mozilla/LazyIdleThread.h"
#include <dlfcn.h>
#include "BluetoothAdapter.h"

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
    ToggleBtResultTask(nsRefPtr<BluetoothAdapter>& adapterPtr, bool result)
      : mResult(result)
    {
      MOZ_ASSERT(!NS_IsMainThread());

      mAdapterPtr.swap(adapterPtr);
    }

    NS_IMETHOD Run() 
    {
      MOZ_ASSERT(NS_IsMainThread());

      if (!mResult) {
        //TODO:Bug-731361
        NS_WARNING("BT firmware loading fails.\n");
      }
 
      //mAdapterPtr must be null before returning to prevent the background 
      //thread from racing to release it during the destruction of this runnable.
      mAdapterPtr->FirePowered();
      mAdapterPtr = nsnull;

      return NS_OK;
    }

  private:
    nsRefPtr<BluetoothAdapter> mAdapterPtr;
    bool mResult;
};

class ToggleBtTask : public nsRunnable
{
  public:
    ToggleBtTask(bool onOff, BluetoothAdapter* adapterPtr)
      : mOnOff(onOff),
        mAdapterPtr(adapterPtr) 
    {
      MOZ_ASSERT(NS_IsMainThread());
    }

    NS_IMETHOD Run() 
    {
      MOZ_ASSERT(!NS_IsMainThread());

      if(!EnsureBluetoothInit()) {
        NS_ERROR("Failed to load bluedroid library.\n");
        return NS_ERROR_FAILURE;
      }

      bool result;

      //Toggle BT here

      if (mOnOff) {
        result = sBluedroidFunctions.bt_enable();
      } else {
        result = sBluedroidFunctions.bt_disable();
      }

      // Create a result thread and pass it to Main Thread, 
      nsCOMPtr<nsIRunnable> resultRunnable = new ToggleBtResultTask(mAdapterPtr, result);

      if (NS_FAILED(NS_DispatchToMainThread(resultRunnable))) {
        NS_WARNING("Failed to dispatch to main thread!");
      }

      return NS_OK;
    }

  private:
    bool mOnOff;
    nsRefPtr<BluetoothAdapter> mAdapterPtr;
};

DOMCI_DATA(BluetoothAdapter, BluetoothAdapter)

NS_IMPL_CYCLE_COLLECTION_CLASS(BluetoothAdapter)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(BluetoothAdapter, 
                                                  nsDOMEventTargetHelper)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(powered)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(BluetoothAdapter, 
                                                nsDOMEventTargetHelper)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(powered)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(BluetoothAdapter)
  NS_INTERFACE_MAP_ENTRY(nsIDOMBluetoothAdapter)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(BluetoothAdapter)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(BluetoothAdapter, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(BluetoothAdapter, nsDOMEventTargetHelper)

BluetoothAdapter::BluetoothAdapter() 
  : mPower(false)
{
}

NS_IMETHODIMP
BluetoothAdapter::GetPower(bool* aPower)
{
#ifdef MOZ_WIDGET_GONK
  if(!EnsureBluetoothInit()) {
    NS_ERROR("Failed to load bluedroid library.\n");
    return NS_ERROR_FAILURE;
  }
  *aPower = sBluedroidFunctions.bt_is_enabled();
#else
  *aPower = mPower;
#endif
  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::SetPower(bool aPower)
{

#ifdef MOZ_WIDGET_GONK
  // Platform specific check for gonk until object is divided in
  // different implementations per platform. Linux doesn't require
  // bluetooth firmware loading, but code should work otherwise.
  if(!EnsureBluetoothInit()) {
    NS_ERROR("Failed to load bluedroid library.\n");
    return NS_ERROR_FAILURE;
  }
#endif
  if (mPower != aPower) {
    mPower = aPower;

#ifdef MOZ_WIDGET_GONK
    return ToggleBluetoothAsync();
#endif
  }

  return NS_OK;
}

nsresult
BluetoothAdapter::ToggleBluetoothAsync()
{
  if (!mToggleBtThread) {
    mToggleBtThread = new LazyIdleThread(15000);
  }

  nsCOMPtr<nsIRunnable> r = new ToggleBtTask(mPower, this);

  return mToggleBtThread->Dispatch(r, NS_DISPATCH_NORMAL);
}

nsresult
BluetoothAdapter::FirePowered()
{
  nsRefPtr<nsDOMEvent> event = new nsDOMEvent(nsnull, nsnull);
  nsresult rv = event->InitEvent(NS_LITERAL_STRING("powered"), false, false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = event->SetTrusted(true);
  NS_ENSURE_SUCCESS(rv, rv);

  bool dummy;
  rv = DispatchEvent(event, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMPL_EVENT_HANDLER(BluetoothAdapter, powered)
