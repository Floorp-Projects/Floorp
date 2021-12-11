/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const ResourceCommand = require("devtools/shared/commands/resource/resource-command");
const { MESSAGE_CATEGORY } = require("devtools/shared/constants");

module.exports = async function({ targetCommand, targetFront, onAvailable }) {
  // Allow the top level target if the targetFront has an `ensureCSSErrorREportingEnabled`
  // function. Also allow frame targets.
  const isAllowed =
    typeof targetFront.ensureCSSErrorReportingEnabled == "function" &&
    (targetFront.isTopLevel ||
      targetFront.targetType === targetCommand.TYPES.FRAME);

  if (!isAllowed) {
    return;
  }

  const webConsoleFront = await targetFront.getFront("console");
  if (webConsoleFront.isDestroyed()) {
    return;
  }

  // Request notifying about new CSS messages (they're emitted from the "PageError listener").
  await webConsoleFront.startListeners(["PageError"]);

  // Fetch already existing messages
  // /!\ The actor implementation requires to call startListeners("PageError") first /!\
  const { messages } = await webConsoleFront.getCachedMessages(["PageError"]);

  const cachedMessages = [];
  for (const message of messages) {
    if (message.pageError?.category !== MESSAGE_CATEGORY.CSS_PARSER) {
      continue;
    }

    message.resourceType = ResourceCommand.TYPES.CSS_MESSAGE;
    message.cssSelectors = message.pageError.cssSelectors;
    delete message.pageError.cssSelectors;
    cachedMessages.push(message);
  }

  onAvailable(cachedMessages);

  // CSS Messages are emited fron the PageError listener, which also send regular errors
  // that we need to filter out.
  webConsoleFront.on("pageError", message => {
    if (message.pageError.category !== MESSAGE_CATEGORY.CSS_PARSER) {
      return;
    }

    message.resourceType = ResourceCommand.TYPES.CSS_MESSAGE;
    message.cssSelectors = message.pageError.cssSelectors;
    delete message.pageError.cssSelectors;
    onAvailable([message]);
  });

  // Calling ensureCSSErrorReportingEnabled will make the server parse the stylesheets to
  // retrieve the warnings if the docShell wasn't already watching for CSS messages.
  await targetFront.ensureCSSErrorReportingEnabled();
};
