/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  TYPES: { COOKIE },
} = require("resource://devtools/shared/commands/resource/resource-command.js");

const { Front, types } = require("resource://devtools/shared/protocol.js");

module.exports = function ({ resource, watcherFront, targetFront }) {
  if (!(resource instanceof Front) && watcherFront) {
    const { innerWindowId } = resource;

    // it's safe to instantiate the front now, so we do it.
    resource = types.getType("cookies").read(resource, targetFront);
    resource.resourceType = COOKIE;
    resource.resourceId = `${COOKIE}-${targetFront.browsingContextID}`;
    resource.resourceKey = "cookies";
    resource.innerWindowId = innerWindowId;
  }

  return resource;
};
