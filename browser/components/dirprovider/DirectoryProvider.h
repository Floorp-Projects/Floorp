/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DirectoryProvider_h__
#define DirectoryProvider_h__

#include "nsIDirectoryService.h"
#include "nsComponentManagerUtils.h"
#include "nsIFile.h"
#include "nsSimpleEnumerator.h"
#include "mozilla/Attributes.h"

#define NS_BROWSERDIRECTORYPROVIDER_CONTRACTID \
  "@mozilla.org/browser/directory-provider;1"

namespace mozilla {
namespace browser {

class DirectoryProvider final : public nsIDirectoryServiceProvider2 {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDIRECTORYSERVICEPROVIDER
  NS_DECL_NSIDIRECTORYSERVICEPROVIDER2

 private:
  ~DirectoryProvider() {}

  class AppendingEnumerator final : public nsSimpleEnumerator {
   public:
    NS_DECL_NSISIMPLEENUMERATOR

    AppendingEnumerator(nsISimpleEnumerator* aBase,
                        char const* const* aAppendList);

   private:
    ~AppendingEnumerator() override = default;

    nsCOMPtr<nsISimpleEnumerator> mBase;
    char const* const* const mAppendList;
    nsCOMPtr<nsIFile> mNext;
  };
};

}  // namespace browser
}  // namespace mozilla

#endif  // DirectoryProvider_h__
