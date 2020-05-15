/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.exports = async function({
  targetList,
  targetType,
  targetFront,
  isTopLevel,
  onAvailable,
}) {
  // Only allow the top level target and processes.
  // Frames can be ignored as logMessage are never sent to them anyway.
  // Also ignore workers as they are not supported yet. (see bug 1592584)
  const isAllowed = isTopLevel || targetType === targetList.TYPES.PROCESS;
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
  const { messages } = await webConsoleFront.getCachedMessages([
    webConsoleFront.traits.newCacheStructure ? "LogMessage" : "PageError",
  ]);

  for (const message of messages) {
    // On older server (< v77), we're also getting pageError cached messages, so we need
    // to ignore those.
    if (
      !webConsoleFront.traits.newCacheStructure &&
      message._type !== "LogMessage"
    ) {
      continue;
    }
    onAvailable(message);
  }

  webConsoleFront.on("logMessage", onAvailable);
};
