/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const ResourceCommand = require("resource://devtools/shared/commands/resource/resource-command.js");

module.exports = async function({ targetCommand, targetFront, onAvailable }) {
  // We only care about the top level parent process target...
  if (!targetFront.isTopLevel) {
    return;
  }
  // ...of the browser console or toolbox.
  const isBrowserConsoleOrToolbox =
    targetCommand.descriptorFront.isBrowserProcessDescriptor;
  if (!isBrowserConsoleOrToolbox) {
    return;
  }

  const webConsoleFront = await targetFront.getFront("console");
  webConsoleFront.on("lastPrivateContextExited", () => {
    onAvailable([
      {
        resourceType: ResourceCommand.TYPES.LAST_PRIVATE_CONTEXT_EXIT,
      },
    ]);
  });
  // Force doing a noop request in order to instantiate the actor.
  // Otherwise it won't be instantiated and won't listen for private browsing events...
  webConsoleFront.startListeners([]);
};
