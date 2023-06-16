/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  TYPES: { EXTENSION_STORAGE },
} = require("resource://devtools/shared/commands/resource/resource-command.js");

const { Front, types } = require("resource://devtools/shared/protocol.js");

module.exports = function ({ resource, watcherFront, targetFront }) {
  if (!(resource instanceof Front) && watcherFront) {
    const { innerWindowId } = resource;

    // it's safe to instantiate the front now, so we do it.
    resource = types.getType("extensionStorage").read(resource, targetFront);
    resource.resourceType = EXTENSION_STORAGE;
    resource.resourceId = `${EXTENSION_STORAGE}-${targetFront.browsingContextID}`;
    resource.resourceKey = "extensionStorage";
    resource.innerWindowId = innerWindowId;
  }

  return resource;
};
