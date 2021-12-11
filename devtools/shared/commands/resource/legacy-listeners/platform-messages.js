/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const ResourceCommand = require("devtools/shared/commands/resource/resource-command");

module.exports = async function({ targetCommand, targetFront, onAvailable }) {
  // Only allow the top level target and processes.
  // Frames can be ignored as logMessage are never sent to them anyway.
  // Also ignore workers as they are not supported yet. (see bug 1592584)
  const isAllowed =
    targetFront.isTopLevel ||
    targetFront.targetType === targetCommand.TYPES.PROCESS;
  if (!isAllowed) {
    return;
  }

  const webConsoleFront = await targetFront.getFront("console");
  if (webConsoleFront.isDestroyed()) {
    return;
  }

  // Request notifying about new messages. Here the "PageError" type start listening for
  // both actual PageErrors (emitted as "pageError" events) as well as LogMessages (
  // emitted as "logMessage" events). This function only set up the listener on the
  // webConsoleFront for "logMessage".
  await webConsoleFront.startListeners(["PageError"]);

  // Fetch already existing messages
  // /!\ The actor implementation requires to call startListeners("PageError") first /!\
  const { messages } = await webConsoleFront.getCachedMessages(["LogMessage"]);

  for (const message of messages) {
    message.resourceType = ResourceCommand.TYPES.PLATFORM_MESSAGE;
  }
  onAvailable(messages);

  webConsoleFront.on("logMessage", message => {
    message.resourceType = ResourceCommand.TYPES.PLATFORM_MESSAGE;
    onAvailable([message]);
  });
};
