/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const ResourceCommand = require("devtools/shared/commands/resource/resource-command");
const { MESSAGE_CATEGORY } = require("devtools/shared/constants");

module.exports = async function({ targetCommand, targetFront, onAvailable }) {
  // Allow the top level target unconditionnally.
  // Also allow frame, but only in content toolbox, i.e. still ignore them in
  // the context of the browser toolbox as we inspect messages via the process
  // targets
  // Also ignore workers as they are not supported yet. (see bug 1592584)
  const listenForFrames = targetCommand.descriptorFront.isLocalTab;
  const isAllowed =
    targetFront.isTopLevel ||
    targetFront.targetType === targetCommand.TYPES.PROCESS ||
    (targetFront.targetType === targetCommand.TYPES.FRAME && listenForFrames);

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
  // webConsoleFront for "pageError".
  await webConsoleFront.startListeners(["PageError"]);

  // Fetch already existing messages
  // /!\ The actor implementation requires to call startListeners("PageError") first /!\
  let { messages } = await webConsoleFront.getCachedMessages(["PageError"]);

  // On server < v79, we're also getting CSS Messages that we need to filter out.
  messages = messages.filter(
    message => message.pageError.category !== MESSAGE_CATEGORY.CSS_PARSER
  );

  messages.forEach(message => {
    message.resourceType = ResourceCommand.TYPES.ERROR_MESSAGE;
  });
  // Cached messages don't have the same shape as live messages,
  // so we need to transform them.
  onAvailable(messages);

  webConsoleFront.on("pageError", message => {
    // On server < v79, we're getting CSS Messages that we need to filter out.
    if (message.pageError.category === MESSAGE_CATEGORY.CSS_PARSER) {
      return;
    }

    message.resourceType = ResourceCommand.TYPES.ERROR_MESSAGE;
    onAvailable([message]);
  });
};
