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
}

RefPtr<GenericPromise>
RemoteSpellcheckEngineChild::SetCurrentDictionaryFromList(
  const nsTArray<nsString>& aList)
{
  MozPromiseHolder<GenericPromise>* promiseHolder =
    new MozPromiseHolder<GenericPromise>();
  if (!SendSetDictionaryFromList(
         aList,
         reinterpret_cast<intptr_t>(promiseHolder))) {
    delete promiseHolder;
    return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }
  // promiseHolder will removed by receive message
  return promiseHolder->Ensure(__func__);
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
  delete promiseHolder;
  return IPC_OK();
}

} //namespace mozilla
