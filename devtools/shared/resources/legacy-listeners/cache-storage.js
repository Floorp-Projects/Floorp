/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  ResourceWatcher,
} = require("devtools/shared/resources/resource-watcher");

const {
  makeStorageLegacyListener,
} = require("devtools/shared/resources/legacy-listeners/storage-utils");

module.exports = makeStorageLegacyListener(
  "Cache",
  ResourceWatcher.TYPES.CACHE_STORAGE
);
