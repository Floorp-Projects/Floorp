/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  ResourceWatcher,
} = require("devtools/shared/resources/resource-watcher");

module.exports = async function({ targetList, targetFront, onAvailable }) {
  // Only allow the top level target and processes.
  // Frames can be ignored as logMessage are never sent to them anyway.
  // Also ignore workers as they are not supported yet. (see bug 1592584)
  const isAllowed =
    targetFront.isTopLevel ||
    targetFront.targetType === targetList.TYPES.PROCESS;
  if (!isAllowed) {
    return;
  }

  const webConsoleFront = await targetFront.getFront("console");

  // Request notifying about new messages. Here the "PageError" type start listening for
  // both actual PageErrors (emitted as "pageError" events) as well as LogMessages (
  // emitted as "logMessage" events). This function only set up the listener on the
  // webConsoleFront for "logMessage".
  await webConsoleFront.startListeners(["PageError"]);

  // Fetch already existing messages
  // /!\ The actor implementation requires to call startListeners("PageError") first /!\
  // On older server (< v77), cached messages have to be retrieved at the same time as
  // PageError messages.
  let { messages } = await webConsoleFront.getCachedMessages([
    webConsoleFront.traits.newCacheStructure ? "LogMessage" : "PageError",
  ]);

  // On older server (< v77), we're also getting pageError cached messages, so we need
  // to ignore those.
  messages = messages.filter(message => {
    return (
      webConsoleFront.traits.newCacheStructure || message._type === "LogMessage"
    );
  });

  for (const message of messages) {
    // Handling cached messages for servers older than Firefox 78.
    if (message._type) {
      delete message._type;
    }
    message.resourceType = ResourceWatcher.TYPES.PLATFORM_MESSAGE;
  }
  onAvailable(messages);

  webConsoleFront.on("logMessage", message => {
    message.resourceType = ResourceWatcher.TYPES.PLATFORM_MESSAGE;
    onAvailable([message]);
  });
};
