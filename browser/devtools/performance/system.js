/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * A dump file to attach preprocessing directives consumable to the controller
 * without littering our code with directives.
 */

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;
const { devtools: loader } = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
const require = loader.require;

const SYSTEM = {};

// If e10s is possible on the platform.
#ifdef E10S_TESTING_ONLY
SYSTEM.MULTIPROCESS_SUPPORTED = true;
#endif
