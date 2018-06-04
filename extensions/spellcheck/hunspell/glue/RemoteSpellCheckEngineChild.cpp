/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/UniquePtr.h"
#include "RemoteSpellCheckEngineChild.h"

namespace mozilla {

RemoteSpellcheckEngineChild::RemoteSpellcheckEngineChild(mozSpellChecker *aOwner)
  : mOwner(aOwner)
{
}

RemoteSpellcheckEngineChild::~RemoteSpellcheckEngineChild()
{
  // null out the owner's SpellcheckEngineChild to prevent state corruption
  // during shutdown
  mOwner->DeleteRemoteEngine();

  // ensure we don't leak any promise holders for which we haven't yet
  // received responses
  for (UniquePtr<MozPromiseHolder<GenericPromise>>& promiseHolder : mResponsePromises) {
    promiseHolder->RejectIfExists(NS_ERROR_ABORT, __func__);
  }
}

RefPtr<GenericPromise>
RemoteSpellcheckEngineChild::SetCurrentDictionaryFromList(
  const nsTArray<nsString>& aList)
{
  UniquePtr<MozPromiseHolder<GenericPromise>> promiseHolder =
    MakeUnique<MozPromiseHolder<GenericPromise>>();
  if (!SendSetDictionaryFromList(
         aList,
         reinterpret_cast<intptr_t>(promiseHolder.get()))) {
    return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }
  RefPtr<GenericPromise> result = promiseHolder->Ensure(__func__);
  // promiseHolder will removed by receive message
  mResponsePromises.AppendElement(std::move(promiseHolder));
  return result;
}

mozilla::ipc::IPCResult
RemoteSpellcheckEngineChild::RecvNotifyOfCurrentDictionary(
                               const nsString& aDictionary,
                               const intptr_t& aId)
{
  MozPromiseHolder<GenericPromise>* promiseHolder =
    reinterpret_cast<MozPromiseHolder<GenericPromise>*>(aId);
  mOwner->mCurrentDictionary = aDictionary;
  if (aDictionary.IsEmpty()) {
    promiseHolder->RejectIfExists(NS_ERROR_NOT_AVAILABLE, __func__);
  } else {
    promiseHolder->ResolveIfExists(true, __func__);
  }
  for (uint32_t i = 0; i < mResponsePromises.Length(); ++i) {
    if (mResponsePromises[i].get() == promiseHolder) {
      mResponsePromises.RemoveElementAt(i);
      break;
    }
  }
  return IPC_OK();
}

} //namespace mozilla
