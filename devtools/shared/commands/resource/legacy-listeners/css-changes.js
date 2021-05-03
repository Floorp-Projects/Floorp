/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  ResourceWatcher,
} = require("devtools/shared/resources/resource-watcher");

module.exports = async function({ targetFront, onAvailable }) {
  if (!targetFront.hasActor("changes")) {
    return;
  }

  const changesFront = await targetFront.getFront("changes");

  // Get all changes collected up to this point by the ChangesActor on the server,
  // then fire each change as "add-change".
  const changes = await changesFront.allChanges();
  await onAvailable(changes.map(change => toResource(change)));

  changesFront.on("add-change", change => onAvailable([toResource(change)]));
};

function toResource(change) {
  return Object.assign(change, {
    resourceType: ResourceWatcher.TYPES.CSS_CHANGE,
  });
}
