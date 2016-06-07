/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef iehistoryenumerator___h___
#define iehistoryenumerator___h___

#include <urlhist.h>

#include "mozilla/Attributes.h"
#include "nsCOMPtr.h"
#include "nsISimpleEnumerator.h"
#include "nsIWritablePropertyBag2.h"

class nsIEHistoryEnumerator final : public nsISimpleEnumerator
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISIMPLEENUMERATOR

  nsIEHistoryEnumerator();

private:
  ~nsIEHistoryEnumerator();

  /**
   * Initializes the history reader, if needed.
   */
  void EnsureInitialized();

  RefPtr<IUrlHistoryStg2> mIEHistory;
  RefPtr<IEnumSTATURL> mURLEnumerator;

  nsCOMPtr<nsIWritablePropertyBag2> mCachedNextEntry;
};

#endif
