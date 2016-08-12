/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FakeInputPortService.h"
#include "InputPortData.h"
#include "mozilla/dom/InputPort.h"
#include "nsIMutableArray.h"
#include "nsITimer.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace dom {

namespace {
class InputPortServiceNotifyRunnable final : public Runnable
{
public:
  InputPortServiceNotifyRunnable(
      nsIInputPortServiceCallback* aCallback,
      nsIArray* aDataList,
      uint16_t aErrorCode = nsIInputPortServiceCallback::INPUTPORT_ERROR_OK)
    : mCallback(aCallback)
    , mDataList(aDataList)
    , mErrorCode(aErrorCode)
  {
    MOZ_ASSERT(aCallback);
    MOZ_ASSERT(aDataList);
  }

  NS_IMETHOD Run() override
  {
    if (mErrorCode == nsIInputPortServiceCallback::INPUTPORT_ERROR_OK) {
      return mCallback->NotifySuccess(mDataList);
    } else {
      return mCallback->NotifyError(mErrorCode);
    }
  }

private:
  nsCOMPtr<nsIInputPortServiceCallback> mCallback;
  nsCOMPtr<nsIArray> mDataList;
  uint16_t mErrorCode;
};

class PortConnectionChangedCallback final : public nsITimerCallback
{
public:
  NS_DECL_ISUPPORTS

  PortConnectionChangedCallback(nsIInputPortData* aInputPortData,
                                nsIInputPortListener* aInputPortListener,
                                const bool aIsConnected)
    : mInputPortData(aInputPortData)
    , mInputPortListener(aInputPortListener)
    , mIsConnected(aIsConnected)
  {}

  NS_IMETHOD
  Notify(nsITimer* aTimer) override
  {
    InputPortData* portData = static_cast<InputPortData*>(mInputPortData.get());
    portData->SetConnected(mIsConnected);
    nsresult rv = mInputPortListener->NotifyConnectionChanged(
      portData->GetId(), mIsConnected);
    return rv;
  }

private:
  ~PortConnectionChangedCallback() {}

  nsCOMPtr<nsIInputPortData> mInputPortData;
  nsCOMPtr<nsIInputPortListener> mInputPortListener;
  bool mIsConnected;
};

} // namespace

NS_IMPL_ISUPPORTS(PortConnectionChangedCallback, nsITimerCallback)

NS_IMPL_CYCLE_COLLECTION_CLASS(FakeInputPortService)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(FakeInputPortService)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mInputPortListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPortConnectionChangedTimer)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPortDatas)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(FakeInputPortService)
  tmp->Shutdown();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mInputPortListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPortConnectionChangedTimer)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPortDatas)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(FakeInputPortService)
NS_IMPL_CYCLE_COLLECTING_RELEASE(FakeInputPortService)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FakeInputPortService)
  NS_INTERFACE_MAP_ENTRY(nsIInputPortService)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

FakeInputPortService::FakeInputPortService()
{
  Init();
}

FakeInputPortService::~FakeInputPortService()
{
  Shutdown();
}

void
FakeInputPortService::Init()
{
  nsCOMPtr<nsIInputPortData> portData1 =
      MockInputPort(NS_LITERAL_STRING("1"), NS_LITERAL_STRING("av"), true);
  mPortDatas.AppendElement(portData1);

  nsCOMPtr<nsIInputPortData> portData2 =
      MockInputPort(NS_LITERAL_STRING("2"), NS_LITERAL_STRING("displayport"), false);
  mPortDatas.AppendElement(portData2);

  nsCOMPtr<nsIInputPortData> portData3 =
      MockInputPort(NS_LITERAL_STRING("3"), NS_LITERAL_STRING("hdmi"), true);
  mPortDatas.AppendElement(portData3);
}

void
FakeInputPortService::Shutdown()
{
  if (mPortConnectionChangedTimer) {
    mPortConnectionChangedTimer->Cancel();
  }
}

NS_IMETHODIMP
FakeInputPortService::GetInputPortListener(nsIInputPortListener** aInputPortListener)
{
  if (!mInputPortListener) {
    *aInputPortListener = nullptr;
    return NS_OK;
  }

  *aInputPortListener = mInputPortListener;
  NS_ADDREF(*aInputPortListener);
  return NS_OK;
}

NS_IMETHODIMP
FakeInputPortService::SetInputPortListener(nsIInputPortListener* aInputPortListener)
{
  mInputPortListener = aInputPortListener;
  return NS_OK;
}

NS_IMETHODIMP
FakeInputPortService::GetInputPorts(nsIInputPortServiceCallback* aCallback)
{
  if (!aCallback) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsIMutableArray> portDataList = do_CreateInstance(NS_ARRAY_CONTRACTID);
  if (!portDataList) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  for (uint32_t i = 0; i < mPortDatas.Length(); i++) {
    portDataList->AppendElement(mPortDatas[i], false);
  }

  mPortConnectionChangedTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
  NS_ENSURE_TRUE(mPortConnectionChangedTimer, NS_ERROR_OUT_OF_MEMORY);
  bool isConnected = false;
  mPortDatas[0]->GetConnected(&isConnected);
  //simulate the connection change event.
  RefPtr<PortConnectionChangedCallback> connectionChangedCb =
    new PortConnectionChangedCallback(mPortDatas[0], mInputPortListener, !isConnected);
  nsresult rv = mPortConnectionChangedTimer->InitWithCallback(
    connectionChangedCb, 100, nsITimer::TYPE_ONE_SHOT);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIRunnable> runnable =
    new InputPortServiceNotifyRunnable(aCallback, portDataList);
  return NS_DispatchToCurrentThread(runnable);
}

already_AddRefed<nsIInputPortData>
FakeInputPortService::MockInputPort(const nsAString& aId,
                                    const nsAString& aType,
                                    bool aIsConnected)
{
  nsCOMPtr<nsIInputPortData> portData = new InputPortData();
  portData->SetId(aId);
  portData->SetType(aType);
  portData->SetConnected(aIsConnected);
  return portData.forget();
}

} // namespace dom
} // namespace mozilla
