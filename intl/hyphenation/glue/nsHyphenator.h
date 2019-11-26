/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHyphenator_h__
#define nsHyphenator_h__

#include "mozilla/ipc/SharedMemoryBasic.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Variant.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsTArray.h"

class nsIURI;
struct HyphDic;

namespace mozilla {
template <>
class DefaultDelete<const HyphDic> {
 public:
  void operator()(const HyphDic* ptr) const;
};
}  // namespace mozilla

class nsHyphenator {
 public:
  nsHyphenator(nsIURI* aURI, bool aHyphenateCapitalized);

  NS_INLINE_DECL_REFCOUNTING(nsHyphenator)

  bool IsValid();

  nsresult Hyphenate(const nsAString& aText, nsTArray<bool>& aHyphens);

  void ShareToProcess(base::ProcessId aPid,
                      mozilla::ipc::SharedMemoryBasic::Handle* aOutHandle,
                      uint32_t* aOutSize);

 private:
  ~nsHyphenator() = default;

  void HyphenateWord(const nsAString& aString, uint32_t aStart, uint32_t aLimit,
                     nsTArray<bool>& aHyphens);

  mozilla::Variant<const void*,  // raw pointer to uncompressed omnijar data
                   RefPtr<mozilla::ipc::SharedMemoryBasic>,  // shmem block
                   mozilla::UniquePtr<const HyphDic>  // loaded by mapped_hyph
                   >
      mDict;
  uint32_t mDictSize;  // size of mDict data (not used if type is HyphDic)
  bool mHyphenateCapitalized;
};

#endif  // nsHyphenator_h__
