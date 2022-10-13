/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const ResourceCommand = require("resource://devtools/shared/commands/resource/resource-command.js");

const {
  makeStorageLegacyListener,
} = require("resource://devtools/shared/commands/resource/legacy-listeners/storage-utils.js");

module.exports = makeStorageLegacyListener(
  "sessionStorage",
  ResourceCommand.TYPES.SESSION_STORAGE
);
