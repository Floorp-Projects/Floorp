/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef edgereadinglistextractor__h__
#define edgereadinglistextractor__h__

#include "nsIArray.h"
#include "nsIEdgeReadingListExtractor.h"

// To get access to the long data types, we need to use at least the Vista version of the JET APIs
#undef JET_VERSION
#define JET_VERSION 0x0600
#include <esent.h>

class nsEdgeReadingListExtractor final : public nsIEdgeReadingListExtractor
{
public:
  nsEdgeReadingListExtractor() {}

  NS_DECL_ISUPPORTS

  NS_DECL_NSIEDGEREADINGLISTEXTRACTOR

private:
  ~nsEdgeReadingListExtractor() {}

  nsresult ConvertJETError(const JET_ERR &err);
};

#endif
