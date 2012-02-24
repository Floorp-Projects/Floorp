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

#include "BluetoothAdapter.h"

#if defined(MOZ_WIDGET_GONK)
#include <bluedroid/bluetooth.h>
#endif

#define POWERED_EVENT_NAME NS_LITERAL_STRING("powered")

BEGIN_BLUETOOTH_NAMESPACE

class ToggleBtResultTask : public nsRunnable
{
  public:
    ToggleBtResultTask(bool result, nsRefPtr<BluetoothAdapter>& adapterPtr)
      : mResult(result)
    {
      MOZ_ASSERT(!NS_IsMainThread()); // This should be running on the worker thread

      mAdapterPtr.swap(adapterPtr);
    }

    NS_IMETHOD Run() {
      MOZ_ASSERT(NS_IsMainThread()); // This method is supposed to run on the main thread!
      mAdapterPtr->FirePowered();

      return NS_OK;
    }

  private:
    bool mResult;
    nsRefPtr<BluetoothAdapter> mAdapterPtr;
};

class ToggleBtTask : public nsRunnable
{
  public:
    ToggleBtTask(bool onOff, BluetoothAdapter* adapterPtr)
      : mOnOff(onOff),
      mAdapterPtr(adapterPtr)
    {
      MOZ_ASSERT(NS_IsMainThread()); // The constructor should be running on the main thread.
    }

    NS_IMETHOD Run() {
      bool result;

      MOZ_ASSERT(!NS_IsMainThread()); // This should be running on the worker thread.

      //Toggle BT here
#if defined(MOZ_WIDGET_GONK)  
      if (mOnOff) {
        result = bt_enable();
      } else {
        result = bt_disable();
      }
#else
      result = true;
#endif

      // Create a result thread and pass it to Main Thread, 
      nsCOMPtr<nsIRunnable> resultRunnable = new ToggleBtResultTask(result, mAdapterPtr);
      NS_DispatchToMainThread(resultRunnable);

      return NS_OK;
    }

  private:
    nsRefPtr<BluetoothAdapter> mAdapterPtr;
    bool mOnOff;
};

END_BLUETOOTH_NAMESPACE

DOMCI_DATA(BluetoothAdapter, mozilla::dom::bluetooth::BluetoothAdapter)

USING_BLUETOOTH_NAMESPACE

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

BluetoothAdapter::BluetoothAdapter() : mPower(false)
{
}

NS_IMETHODIMP
BluetoothAdapter::GetPower(bool* aPower)
{
#if defined(MOZ_WIDGET_GONK)  
  *aPower = bt_is_enabled();
#else
  *aPower = mPower;
#endif
  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::SetPower(bool aPower)
{
  if (mPower != aPower) {
    mPower = aPower;

    ToggleBluetoothAsync();
  }

  return NS_OK;
}

void 
BluetoothAdapter::ToggleBluetoothAsync()
{
  if (!mToggleBtThread) {
    mToggleBtThread = new LazyIdleThread(15000);
  }

  nsCOMPtr<nsIRunnable> r = new ToggleBtTask(mPower, this);

  mToggleBtThread->Dispatch(r, 0);
}

nsresult
BluetoothAdapter::FirePowered()
{
  nsRefPtr<nsDOMEvent> event = new nsDOMEvent(nsnull, nsnull);
  nsresult rv = event->InitEvent(POWERED_EVENT_NAME, false, false);
  NS_ENSURE_SUCCESS(rv, rv);

  bool dummy;
  rv = DispatchEvent(event, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMPL_EVENT_HANDLER(BluetoothAdapter, powered)
