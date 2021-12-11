/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  TYPES: { INDEXED_DB },
} = require("devtools/shared/commands/resource/resource-command");

const { Front, types } = require("devtools/shared/protocol.js");

module.exports = function({ resource, watcherFront, targetFront }) {
  if (!(resource instanceof Front) && watcherFront) {
    const { innerWindowId } = resource;

    // it's safe to instantiate the front now, so we do it.
    resource = types.getType("indexedDB").read(resource, targetFront);
    resource.resourceType = INDEXED_DB;
    resource.resourceId = `${INDEXED_DB}-${targetFront.browsingContextID}`;
    resource.resourceKey = "indexedDB";
    resource.innerWindowId = innerWindowId;
  }

  return resource;
};
