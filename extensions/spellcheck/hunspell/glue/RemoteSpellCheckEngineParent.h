/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef RemoteSpellcheckEngineParent_h_
#define RemoteSpellcheckEngineParent_h_

#include "mozilla/PRemoteSpellcheckEngineParent.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"

class mozSpellChecker;

namespace mozilla {

class RemoteSpellcheckEngineParent : public PRemoteSpellcheckEngineParent {
 public:
  RemoteSpellcheckEngineParent();

  virtual ~RemoteSpellcheckEngineParent();

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  virtual mozilla::ipc::IPCResult RecvSetDictionary(
      const nsCString& aDictionary, bool* success);

  virtual mozilla::ipc::IPCResult RecvSetDictionaryFromList(
      nsTArray<nsCString>&& aList, SetDictionaryFromListResolver&& aResolve);

  virtual mozilla::ipc::IPCResult RecvCheckAsync(nsTArray<nsString>&& aWord,
                                                 CheckAsyncResolver&& aResolve);

  virtual mozilla::ipc::IPCResult RecvSuggest(const nsString& aWord,
                                              uint32_t aCount,
                                              SuggestResolver&& aResolve);

 private:
  RefPtr<mozSpellChecker> mSpellChecker;
};

}  // namespace mozilla

#endif
