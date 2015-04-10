/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InputPortServiceFactory.h"
#include "mozilla/dom/InputPort.h"
#include "mozilla/dom/InputPortManager.h"
#include "mozilla/dom/InputPortManagerBinding.h"
#include "mozilla/dom/Promise.h"
#include "nsArrayUtils.h"
#include "nsIInputPortService.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(InputPortManager,
                                      mParent,
                                      mInputPortService,
                                      mPendingGetInputPortsPromises,
                                      mInputPorts)

NS_IMPL_CYCLE_COLLECTING_ADDREF(InputPortManager)
NS_IMPL_CYCLE_COLLECTING_RELEASE(InputPortManager)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(InputPortManager)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIInputPortServiceCallback)
NS_INTERFACE_MAP_END

InputPortManager::InputPortManager(nsPIDOMWindow* aWindow)
  : mParent(aWindow)
  , mIsReady(false)
{
}

InputPortManager::~InputPortManager()
{
}

/* static */ already_AddRefed<InputPortManager>
InputPortManager::Create(nsPIDOMWindow* aWindow, ErrorResult& aRv)
{
  nsRefPtr<InputPortManager> manager = new InputPortManager(aWindow);
  manager->Init(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }
  return manager.forget();
}

void
InputPortManager::Init(ErrorResult& aRv)
{
  mInputPortService = InputPortServiceFactory::AutoCreateInputPortService();
  if (NS_WARN_IF(!mInputPortService)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  aRv = mInputPortService->GetInputPorts(this);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
}

nsPIDOMWindow*
InputPortManager::GetParentObject() const
{
  return mParent;
}

JSObject*
InputPortManager::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return InputPortManagerBinding::Wrap(aCx, this, aGivenProto);
}

void
InputPortManager::RejectPendingGetInputPortsPromises(nsresult aRv)
{
  // Reject pending promises.
  uint32_t length = mPendingGetInputPortsPromises.Length();
  for(uint32_t i = 0; i < length; i++) {
    mPendingGetInputPortsPromises[i]->MaybeReject(aRv);
  }
  mPendingGetInputPortsPromises.Clear();
}

nsresult
InputPortManager::SetInputPorts(const nsTArray<nsRefPtr<InputPort>>& aPorts)
{
  MOZ_ASSERT(!mIsReady);
  // Should be called only when InputPortManager hasn't been ready yet.
  if (mIsReady) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  mInputPorts = aPorts;
  mIsReady = true;

  // Resolve pending promises.
  uint32_t length = mPendingGetInputPortsPromises.Length();
  for(uint32_t i = 0; i < length; i++) {
    mPendingGetInputPortsPromises[i]->MaybeResolve(mInputPorts);
  }
  mPendingGetInputPortsPromises.Clear();
  return NS_OK;
}

already_AddRefed<Promise>
InputPortManager::GetInputPorts(ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  MOZ_ASSERT(global);

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  if (mIsReady) {
    promise->MaybeResolve(mInputPorts);
  } else {
    mPendingGetInputPortsPromises.AppendElement(promise);
  }

  return promise.forget();
}

NS_IMETHODIMP
InputPortManager::NotifySuccess(nsIArray* aDataList)
{
  MOZ_ASSERT(aDataList);

  if (!aDataList) {
    RejectPendingGetInputPortsPromises(NS_ERROR_DOM_ABORT_ERR);
    return NS_ERROR_INVALID_ARG;
  }

  uint32_t length;
  nsresult rv = aDataList->GetLength(&length);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIInputPortListener> portListener;
  rv = mInputPortService->GetInputPortListener(
    getter_AddRefs(portListener));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  ErrorResult erv;
  nsTArray<nsRefPtr<InputPort>> ports(length);
  for (uint32_t i = 0; i < length; i++) {
    nsCOMPtr<nsIInputPortData> portData = do_QueryElementAt(aDataList, i);
    if (NS_WARN_IF(!portData)) {
      continue;
    }

    InputPortData* data = static_cast<InputPortData*>(portData.get());
    nsRefPtr<InputPort> port;
    switch (data->GetType()) {
    case InputPortType::Av:
      port = AVInputPort::Create(GetParentObject(), portListener,
                                 portData, erv);
      break;
    case InputPortType::Displayport:
      port = DisplayPortInputPort::Create(GetParentObject(),
                                          portListener, portData, erv);
      break;
    case InputPortType::Hdmi:
      port = HDMIInputPort::Create(GetParentObject(), portListener,
                                   portData, erv);
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown InputPort type");
      break;
    }
    MOZ_ASSERT(port);

    ports.AppendElement(port);
  }

  if (NS_WARN_IF(erv.Failed())) {
    return erv.ErrorCode();
  }

  erv = SetInputPorts(ports);

  return erv.ErrorCode();
}

NS_IMETHODIMP
InputPortManager::NotifyError(uint16_t aErrorCode)
{
  switch (aErrorCode) {
  case nsIInputPortServiceCallback::INPUTPORT_ERROR_FAILURE:
  case nsIInputPortServiceCallback::INPUTPORT_ERROR_INVALID_ARG:
    RejectPendingGetInputPortsPromises(NS_ERROR_DOM_ABORT_ERR);
    return NS_OK;
  case nsIInputPortServiceCallback::INPUTPORT_ERROR_NOT_SUPPORTED:
    RejectPendingGetInputPortsPromises(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return NS_OK;
  }

  RejectPendingGetInputPortsPromises(NS_ERROR_DOM_ABORT_ERR);
  return NS_ERROR_ILLEGAL_VALUE;
}

} // namespace dom
} // namespace mozilla
