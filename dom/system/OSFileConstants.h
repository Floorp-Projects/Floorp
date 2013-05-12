/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_osfileconstants_h__
#define mozilla_osfileconstants_h__

#include "jspubtd.h"
#include "nsIOSFileConstantsService.h"
#include "mozilla/Attributes.h"

namespace mozilla {

/**
 * Perform initialization of this module.
 *
 * This function _must_ be called:
 * - from the main thread;
 * - before any Chrome Worker is created.
 *
 * The function is idempotent.
 */
nsresult InitOSFileConstants();

/**
 * Perform cleanup of this module.
 *
 * This function _must_ be called:
 * - from the main thread;
 * - after all Chrome Workers are dead.
 *
 * The function is idempotent.
 */
void CleanupOSFileConstants();

/**
 * Define OS-specific constants.
 *
 * This function creates or uses JS object |OS.Constants| to store
 * all its constants.
 */
bool DefineOSFileConstants(JSContext *cx, JS::Handle<JSObject*> global);

/**
 * XPConnect initializer, for use in the main thread.
 */
class OSFileConstantsService MOZ_FINAL : public nsIOSFileConstantsService
{
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOSFILECONSTANTSSERVICE
  OSFileConstantsService();
private:
  ~OSFileConstantsService();
};

}

#endif // mozilla_osfileconstants_h__
