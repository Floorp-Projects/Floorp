/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const ResourceCommand = require("devtools/shared/commands/resource/resource-command");

const Services = require("Services");

// This legacy listener is used to retrieve content messages that are cloned from content
// process to the parent process for BrowserConsole and BrowserToolbox when multiprocess
// support is disabled.
module.exports = async function({ targetCommand, targetFront, onAvailable }) {
  const browserToolboxFissionSupport = Services.prefs.getBoolPref(
    "devtools.browsertoolbox.fission",
    false
  );

  // Content process messages should only be retrieved for top-level browser console and
  // browser toolbox targets, when fission support is disabled.
  const acceptTarget =
    targetFront.isParentProcess &&
    !targetFront.isAddon &&
    !browserToolboxFissionSupport;

  if (!acceptTarget) {
    return;
  }

  const webConsoleFront = await targetFront.getFront("console");
  if (webConsoleFront.isDestroyed()) {
    return;
  }

  // Request notifying about new messages
  await webConsoleFront.startListeners(["ContentProcessMessages"]);

  // ContentProcessMessages are sent through the consoleAPICall event, and are seen as
  // console api messages on the client.
  webConsoleFront.on("consoleAPICall", packet => {
    // Ignore console messages that aren't from the content process
    if (!packet.clonedFromContentProcess) {
      return;
    }
    packet.resourceType = ResourceCommand.TYPES.CONSOLE_MESSAGE;
    onAvailable([packet]);
  });
};
