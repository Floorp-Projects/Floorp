
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsedgemigrationutils__h__
#define nsedgemigrationutils__h__

#include "nsISupportsImpl.h"
#include "nsIEdgeMigrationUtils.h"

namespace mozilla {

class nsEdgeMigrationUtils final : public nsIEdgeMigrationUtils {
  NS_DECL_ISUPPORTS
  NS_DECL_NSIEDGEMIGRATIONUTILS

 private:
  ~nsEdgeMigrationUtils() = default;
};

}  // namespace mozilla

#endif
