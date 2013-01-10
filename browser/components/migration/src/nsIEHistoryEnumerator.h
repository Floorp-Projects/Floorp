/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef iehistoryenumerator___h___
#define iehistoryenumerator___h___

#include <urlhist.h>

#include "mozilla/Attributes.h"
#include "nsISimpleEnumerator.h"
#include "nsIWritablePropertyBag2.h"
#include "nsAutoPtr.h"

class nsIEHistoryEnumerator MOZ_FINAL : public nsISimpleEnumerator
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

  nsRefPtr<IUrlHistoryStg2> mIEHistory;
  nsRefPtr<IEnumSTATURL> mURLEnumerator;

  nsCOMPtr<nsIWritablePropertyBag2> mCachedNextEntry;
};

#endif
