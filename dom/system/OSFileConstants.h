/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_osfileconstants_h__
#define mozilla_osfileconstants_h__

#include "jspubtd.h"

namespace mozilla {

/**
 * Perform initialization of this module.
 *
 * This function _must_ be called:
 * - from the main thread;
 * - only once;
 * - before any Chrome Worker is created.
 */
nsresult InitOSFileConstants();

/**
 * Perform cleanup of this module.
 *
 * This function _must_ be called:
 * - from the main thread;
 * - only once;
 * - after all Chrome Workers are dead.
 */
nsresult CleanupOSFileConstants();

/**
 * Define OS-specific constants.
 *
 * This function creates or uses JS object |OS.Constants| to store
 * all its constants.
 */
bool DefineOSFileConstants(JSContext *cx, JSObject *global);

}

#endif // mozilla_osfileconstants_h__
