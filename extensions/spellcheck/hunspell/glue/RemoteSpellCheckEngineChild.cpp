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
  RefPtr<GenericPromise> promise = promiseHolder->Ensure(__func__);
  RefPtr<mozSpellChecker> spellChecker = mOwner;

  SendSetDictionaryFromList(aList)->Then(
    GetMainThreadSerialEventTarget(),
    __func__,
    [spellChecker, promiseHolder](const Tuple<bool, nsString>& aParam) {
      UniquePtr<MozPromiseHolder<GenericPromise>> holder(promiseHolder);
      if (!Get<0>(aParam)) {
        spellChecker->mCurrentDictionary.Truncate();
        holder->Reject(NS_ERROR_NOT_AVAILABLE, __func__);
        return;
      }
      spellChecker->mCurrentDictionary = Get<1>(aParam);
      holder->Resolve(true, __func__);
    },
    [spellChecker, promiseHolder](ResponseRejectReason aReason) {
      UniquePtr<MozPromiseHolder<GenericPromise>> holder(promiseHolder);
      spellChecker->mCurrentDictionary.Truncate();
      holder->Reject(NS_ERROR_NOT_AVAILABLE, __func__);
    });
  return promise;
}

} //namespace mozilla
