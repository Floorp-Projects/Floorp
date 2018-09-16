/* vim: set ts=2 sw=2 sts=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteSpellCheckEngineParent.h"
#include "mozilla/Unused.h"
#include "mozilla/mozSpellChecker.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {

RemoteSpellcheckEngineParent::RemoteSpellcheckEngineParent()
{
  mSpellChecker = mozSpellChecker::Create();
}

RemoteSpellcheckEngineParent::~RemoteSpellcheckEngineParent()
{
}

mozilla::ipc::IPCResult
RemoteSpellcheckEngineParent::RecvSetDictionary(
  const nsString& aDictionary,
  bool* success)
{
  nsresult rv = mSpellChecker->SetCurrentDictionary(aDictionary);
  *success = NS_SUCCEEDED(rv);
  return IPC_OK();
}

mozilla::ipc::IPCResult
RemoteSpellcheckEngineParent::RecvSetDictionaryFromList(
                                nsTArray<nsString>&& aList,
                                SetDictionaryFromListResolver&& aResolve)
{
  for (auto& dictionary : aList) {
    nsresult rv = mSpellChecker->SetCurrentDictionary(dictionary);
    if (NS_SUCCEEDED(rv)) {
      aResolve(Tuple<const bool&, const nsString&>(true, dictionary));
      return IPC_OK();
    }
  }
  aResolve(Tuple<const bool&, const nsString&>(false, EmptyString()));
  return IPC_OK();
}

mozilla::ipc::IPCResult
RemoteSpellcheckEngineParent::RecvCheck(
  const nsString& aWord,
  bool* aIsMisspelled)
{
  nsresult rv = mSpellChecker->CheckWord(aWord, aIsMisspelled, nullptr);

  // If CheckWord failed, we can't tell whether the word is correctly spelled.
  if (NS_FAILED(rv))
    *aIsMisspelled = false;
  return IPC_OK();
}

mozilla::ipc::IPCResult
RemoteSpellcheckEngineParent::RecvCheckAndSuggest(
  const nsString& aWord,
  bool* aIsMisspelled,
  InfallibleTArray<nsString>* aSuggestions)
{
  nsresult rv = mSpellChecker->CheckWord(aWord, aIsMisspelled, aSuggestions);
  if (NS_FAILED(rv)) {
    aSuggestions->Clear();
    *aIsMisspelled = false;
  }
  return IPC_OK();
}

void
RemoteSpellcheckEngineParent::ActorDestroy(ActorDestroyReason aWhy)
{
}

} // namespace mozilla
