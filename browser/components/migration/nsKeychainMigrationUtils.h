/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsKeychainMigrationUtils_h__
#define nsKeychainMigrationUtils_h__

#include <CoreFoundation/CoreFoundation.h>

#include "nsIKeychainMigrationUtils.h"

class nsKeychainMigrationUtils : public nsIKeychainMigrationUtils {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIKEYCHAINMIGRATIONUTILS

  nsKeychainMigrationUtils(){};

 protected:
  virtual ~nsKeychainMigrationUtils(){};
};

#endif
