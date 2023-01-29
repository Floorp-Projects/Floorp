/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  TYPES: { EXTENSION_STORAGE },
} = require("resource://devtools/server/actors/resources/index.js");

const ParentProcessStorage = require("resource://devtools/server/actors/resources/utils/parent-process-storage.js");
const {
  ExtensionStorageActor,
} = require("resource://devtools/server/actors/resources/storage/extension-storage.js");

class ExtensionStorageWatcher extends ParentProcessStorage {
  constructor() {
    super(ExtensionStorageActor, "extensionStorage", EXTENSION_STORAGE);
  }
  async watch(watcherActor, { onAvailable }) {
    if (watcherActor.sessionContext.type != "webextension") {
      throw new Error(
        "EXTENSION_STORAGE should only be listened when debugging a webextension"
      );
    }
    return super.watch(watcherActor, { onAvailable });
  }
}

module.exports = ExtensionStorageWatcher;
