/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  ResourceWatcher,
} = require("devtools/shared/resources/resource-watcher");

module.exports = async function({
  targetList,
  targetFront,
  isFissionEnabledOnContentToolbox,
  onAvailable,
}) {
  // Allow the top level target unconditionnally.
  // Also allow frame, but only in content toolbox, when the fission/content toolbox pref is
  // set. i.e. still ignore them in the content of the browser toolbox as we inspect
  // messages via the process targets
  // Also ignore workers as they are not supported yet. (see bug 1592584)
  const isContentToolbox = targetList.targetFront.isLocalTab;
  const listenForFrames = isContentToolbox && isFissionEnabledOnContentToolbox;
  const isAllowed =
    targetFront.isTopLevel ||
    targetFront.targetType === targetList.TYPES.PROCESS ||
    (targetFront.targetType === targetList.TYPES.FRAME && listenForFrames);

  if (!isAllowed) {
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
  let { messages } = await webConsoleFront.getCachedMessages(["ConsoleAPI"]);

  messages = messages.map(message => {
    // Handling cached messages for servers older than Firefox 78.
    // Wrap the message into a `message` attribute, to match `consoleAPICall` behavior
    if (message._type) {
      return {
        message,
        resourceType: ResourceWatcher.TYPES.CONSOLE_MESSAGE,
      };
    }
    message.resourceType = ResourceWatcher.TYPES.CONSOLE_MESSAGE;
    return message;
  });
  onAvailable(messages);

  // Forward new message events
  webConsoleFront.on("consoleAPICall", message => {
    message.resourceType = ResourceWatcher.TYPES.CONSOLE_MESSAGE;
    onAvailable([message]);
  });
};
