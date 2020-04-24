/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");

module.exports = async function({
  targetList,
  targetType,
  targetFront,
  isTopLevel,
  onAvailable,
}) {
  // Allow the top level target unconditionnally.
  // Also allow frame, but only in content toolbox, when the fission/content toolbox pref is
  // set. i.e. still ignore them in the content of the browser toolbox as we inspect
  // messages via the process targets
  // Also ignore workers as they are not supported yet. (see bug 1592584)
  const isContentToolbox = targetList.targetFront.isLocalTab;
  const listenForFrames =
    isContentToolbox &&
    Services.prefs.getBoolPref("devtools.contenttoolbox.fission");
  const isAllowed =
    isTopLevel ||
    targetType === targetList.TYPES.PROCESS ||
    (targetType === targetList.TYPES.FRAME && listenForFrames);

  if (!isAllowed) {
    return;
  }

  const webConsoleFront = await targetFront.getFront("console");

  // Request notifying about new messages
  await webConsoleFront.startListeners(["ConsoleAPI"]);

  // Fetch already existing messages
  // /!\ The actor implementation requires to call startListeners(ConsoleAPI) first /!\
  const { messages } = await webConsoleFront.getCachedMessages(["ConsoleAPI"]);
  // Wrap the message into a `message` attribute, to match `consoleAPICall` behavior
  messages.map(message => ({ message })).forEach(onAvailable);

  // Forward new message events
  webConsoleFront.on("consoleAPICall", onAvailable);
};
