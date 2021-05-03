/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  TYPES: { SESSION_STORAGE },
} = require("devtools/shared/resources/resource-watcher");

const { Front, types } = require("devtools/shared/protocol.js");

module.exports = function({ resource, watcherFront, targetFront }) {
  if (!(resource instanceof Front) && watcherFront) {
    // instantiate front for session storage
    resource = types.getType("sessionStorage").read(resource, targetFront);
    resource.resourceType = SESSION_STORAGE;
    resource.resourceId = SESSION_STORAGE;
    resource.resourceKey = "sessionStorage";
  }

  return resource;
};
