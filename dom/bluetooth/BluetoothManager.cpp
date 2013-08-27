/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"
#include "BluetoothManager.h"
#include "BluetoothCommon.h"
#include "BluetoothAdapter.h"
#include "BluetoothService.h"
#include "BluetoothReplyRunnable.h"

#include "DOMRequest.h"
#include "nsContentUtils.h"
#include "nsDOMClassInfo.h"
#include "nsIPermissionManager.h"
#include "nsThreadUtils.h"
#include "mozilla/Util.h"
#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "mozilla/dom/BluetoothManagerBinding.h"

using namespace mozilla;

USING_BLUETOOTH_NAMESPACE

// QueryInterface implementation for BluetoothManager
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(BluetoothManager)
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
  ParseSuccessfulReply(JS::Value* aValue)
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
    nsRefPtr<BluetoothAdapter> adapter =
      BluetoothAdapter::Create(mManagerPtr->GetOwner(), values);

    nsresult rv;
    nsIScriptContext* sc = mManagerPtr->GetContextForEventHandlers(&rv);
    if (!sc) {
      NS_WARNING("Cannot create script context!");
      SetError(NS_LITERAL_STRING("BluetoothScriptContextError"));
      return false;
    }

    AutoPushJSContext cx(sc->GetNativeContext());

    JS::Rooted<JSObject*> global(cx, sc->GetWindowProxy());
    rv = nsContentUtils::WrapNative(cx, global, adapter, aValue);
    if (NS_FAILED(rv)) {
      NS_WARNING("Cannot create native object!");
      SetError(NS_LITERAL_STRING("BluetoothNativeObjectError"));
      return false;
    }

    return true;
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

BluetoothManager::BluetoothManager(nsPIDOMWindow *aWindow)
  : nsDOMEventTargetHelper(aWindow)
  , BluetoothPropertyContainer(BluetoothObjectType::TYPE_MANAGER)
{
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(IsDOMBinding());

  BindToOwner(aWindow);
  mPath.AssignLiteral("/");

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);
  bs->RegisterBluetoothSignalHandler(NS_LITERAL_STRING(KEY_MANAGER), this);
}

BluetoothManager::~BluetoothManager()
{
  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);
  bs->UnregisterBluetoothSignalHandler(NS_LITERAL_STRING(KEY_MANAGER), this);
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

bool
BluetoothManager::GetEnabled(ErrorResult& aRv)
{
  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    aRv.Throw(NS_ERROR_FAILURE);
    return false;
  }

  return bs->IsEnabled();
}

already_AddRefed<dom::DOMRequest>
BluetoothManager::GetDefaultAdapter(ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(win);
  nsRefPtr<BluetoothReplyRunnable> results =
    new GetAdapterTask(this, request);

  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsresult rv = bs->GetDefaultAdapterPathInternal(results);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

// static
already_AddRefed<BluetoothManager>
BluetoothManager::Create(nsPIDOMWindow* aWindow)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);

  nsRefPtr<BluetoothManager> manager = new BluetoothManager(aWindow);
  return manager.forget();
}

// static
bool
BluetoothManager::CheckPermission(nsPIDOMWindow* aWindow)
{
  NS_ASSERTION(aWindow, "Null pointer!");

  nsCOMPtr<nsIPermissionManager> permMgr =
    do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);
  NS_ENSURE_TRUE(permMgr, false);

  uint32_t permission;
  nsresult rv =
    permMgr->TestPermissionFromWindow(aWindow, "bluetooth",
                                      &permission);
  NS_ENSURE_SUCCESS(rv, false);

  return permission == nsIPermissionManager::ALLOW_ACTION;
}

void
BluetoothManager::Notify(const BluetoothSignal& aData)
{
  BT_LOG("[M] %s: %s", __FUNCTION__, NS_ConvertUTF16toUTF8(aData.name()).get());

  if (aData.name().EqualsLiteral("AdapterAdded")) {
    DispatchTrustedEvent(NS_LITERAL_STRING("adapteradded"));
  } else if (aData.name().EqualsLiteral("Enabled")) {
    DispatchTrustedEvent(NS_LITERAL_STRING("enabled"));
  } else if (aData.name().EqualsLiteral("Disabled")) {
    DispatchTrustedEvent(NS_LITERAL_STRING("disabled"));
  } else {
#ifdef DEBUG
    nsCString warningMsg;
    warningMsg.AssignLiteral("Not handling manager signal: ");
    warningMsg.Append(NS_ConvertUTF16toUTF8(aData.name()));
    NS_WARNING(warningMsg.get());
#endif
  }
}

bool
BluetoothManager::IsConnected(uint16_t aProfileId, ErrorResult& aRv)
{
  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    aRv.Throw(NS_ERROR_FAILURE);
    return false;
  }

  return bs->IsConnected(aProfileId);
}

JSObject*
BluetoothManager::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return BluetoothManagerBinding::Wrap(aCx, aScope, this);
}
