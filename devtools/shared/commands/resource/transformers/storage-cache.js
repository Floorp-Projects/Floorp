/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  TYPES: { CACHE_STORAGE },
} = require("devtools/shared/commands/resource/resource-command");

const { Front, types } = require("devtools/shared/protocol.js");

module.exports = function({ resource, watcherFront, targetFront }) {
  if (!(resource instanceof Front) && watcherFront) {
    // instantiate front for local storage
    resource = types.getType("Cache").read(resource, targetFront);
    resource.resourceType = CACHE_STORAGE;
    resource.resourceId = CACHE_STORAGE;
    resource.resourceKey = "Cache";
  }

  return resource;
};
