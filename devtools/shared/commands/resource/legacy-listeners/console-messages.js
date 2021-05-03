/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const ResourceCommand = require("devtools/shared/commands/resource/resource-command");

module.exports = async function({ targetCommand, targetFront, onAvailable }) {
  // Allow the top level target unconditionnally.
  // Also allow frame, but only in content toolbox, i.e. still ignore them in
  // the context of the browser toolbox as we inspect messages via the process
  // targets
  const listenForFrames = targetCommand.descriptorFront.isLocalTab;

  // Allow workers when messages aren't dispatched to the main thread.
  const listenForWorkers = !targetCommand.rootFront.traits
    .workerConsoleApiMessagesDispatchedToMainThread;

  const acceptTarget =
    targetFront.isTopLevel ||
    targetFront.targetType === targetCommand.TYPES.PROCESS ||
    (targetFront.targetType === targetCommand.TYPES.FRAME && listenForFrames) ||
    (targetFront.targetType === targetCommand.TYPES.WORKER && listenForWorkers);

  if (!acceptTarget) {
    return;
  }

  const webConsoleFront = await targetFront.getFront("console");
  if (webConsoleFront.isDestroyed()) {
    return;
  }

  // Request notifying about new messages
  await webConsoleFront.startListeners(["ConsoleAPI"]);

  // Fetch already existing messages
  // /!\ The actor implementation requires to call startListeners(ConsoleAPI) first /!\
  const { messages } = await webConsoleFront.getCachedMessages(["ConsoleAPI"]);

  for (const message of messages) {
    message.resourceType = ResourceCommand.TYPES.CONSOLE_MESSAGE;
  }
  onAvailable(messages);

  // Forward new message events
  webConsoleFront.on("consoleAPICall", message => {
    // Ignore console messages that are cloned from the content process (they're handled
    // by the cloned-content-process-messages legacy listener).
    if (message.clonedFromContentProcess) {
      return;
    }

    message.resourceType = ResourceCommand.TYPES.CONSOLE_MESSAGE;
    onAvailable([message]);
  });
};
