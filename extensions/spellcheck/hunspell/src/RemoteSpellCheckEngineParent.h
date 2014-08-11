/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef RemoteSpellcheckEngineParent_h_
#define RemoteSpellcheckEngineParent_h_

#include "mozISpellCheckingEngine.h"
#include "mozilla/PRemoteSpellcheckEngineParent.h"
#include "nsCOMPtr.h"

namespace mozilla {

class RemoteSpellcheckEngineParent : public mozilla::PRemoteSpellcheckEngineParent {

public:
  RemoteSpellcheckEngineParent();

  ~RemoteSpellcheckEngineParent();

  virtual void ActorDestroy(ActorDestroyReason aWhy);

  bool AnswerSetDictionary(const nsString& aDictionary, bool* success);

  bool AnswerCheck( const nsString& aWord, bool* aIsMisspelled);

  bool AnswerCheckAndSuggest(
            const nsString& aWord,
            bool* aIsMisspelled,
            InfallibleTArray<nsString>* aSuggestions);



private:
  nsCOMPtr<mozISpellCheckingEngine> mEngine;
};

}
#endif
