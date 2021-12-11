/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RemoteSpellcheckEngineChild_h_
#define RemoteSpellcheckEngineChild_h_

#include "mozilla/MozPromise.h"
#include "mozilla/PRemoteSpellcheckEngineChild.h"
#include "mozSpellChecker.h"

class mozSpellChecker;

namespace mozilla {

class RemoteSpellcheckEngineChild
    : public mozilla::PRemoteSpellcheckEngineChild {
 public:
  explicit RemoteSpellcheckEngineChild(mozSpellChecker* aOwner);
  virtual ~RemoteSpellcheckEngineChild();

  RefPtr<GenericPromise> SetCurrentDictionaryFromList(
      const nsTArray<nsCString>& aList);

  RefPtr<CheckWordPromise> CheckWords(const nsTArray<nsString>& aWords);

 private:
  mozSpellChecker* mOwner;
};

}  // namespace mozilla

#endif  // RemoteSpellcheckEngineChild_h_
