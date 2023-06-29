/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  formatDisplayName,
} = require("resource://devtools/server/actors/frame.js");
const {
  TYPES,
  getResourceWatcher,
} = require("resource://devtools/server/actors/resources/index.js");

// Get a string message to display when a frame evaluation throws.
function getThrownMessage(completion) {
  try {
    if (completion.throw.getOwnPropertyDescriptor) {
      return completion.throw.getOwnPropertyDescriptor("message").value;
    } else if (completion.toString) {
      return completion.toString();
    }
  } catch (ex) {
    // ignore
  }
  return "Unknown exception";
}
module.exports.getThrownMessage = getThrownMessage;

function logEvent({ threadActor, frame, level, expression, bindings }) {
  const { sourceActor, line, column } =
    threadActor.sourcesManager.getFrameLocation(frame);
  const displayName = formatDisplayName(frame);

  // TODO remove this branch when (#1592584) lands (#1609540)
  if (isWorker) {
    threadActor._parent._consoleActor.evaluateJS({
      text: `console.log(...${expression})`,
      bindings: { displayName, ...bindings },
      url: sourceActor.url,
      lineNumber: line,
      disableBreaks: true,
    });

    return undefined;
  }

  let completion;
  // Ensure disabling all types of breakpoints for all sources while evaluating the log points
  threadActor.insideClientEvaluation = { disableBreaks: true };
  try {
    completion = frame.evalWithBindings(
      expression,
      {
        displayName,
        ...bindings,
      },
      { hideFromDebugger: true }
    );
  } finally {
    threadActor.insideClientEvaluation = null;
  }

  let value;
  if (!completion) {
    // The evaluation was killed (possibly by the slow script dialog).
    value = ["Evaluation failed"];
  } else if ("return" in completion) {
    value = completion.return;
  } else {
    value = [getThrownMessage(completion)];
    level = `${level}Error`;
  }

  if (value && typeof value.unsafeDereference === "function") {
    value = value.unsafeDereference();
  }

  const targetActor = threadActor._parent;
  const message = {
    filename: sourceActor.url,
    lineNumber: line,
    columnNumber: column,
    arguments: value,
    level,
    timeStamp: ChromeUtils.dateNow(),
    chromeContext:
      targetActor.actorID &&
      /conn\d+\.parentProcessTarget\d+/.test(targetActor.actorID),
    // The 'prepareConsoleMessageForRemote' method in webconsoleActor expects internal source ID,
    // thus we can't set sourceId directly to sourceActorID.
    sourceId: sourceActor.internalSourceId,
  };

  // Note that only WindowGlobalTarget actor support resource watcher
  // This is still missing for worker and content processes
  const consoleMessageWatcher = getResourceWatcher(
    targetActor,
    TYPES.CONSOLE_MESSAGE
  );
  if (consoleMessageWatcher) {
    consoleMessageWatcher.emitMessages([message]);
  } else {
    // Bug 1642296: Once we enable ConsoleMessage resource on the server, we should remove onConsoleAPICall
    // from the WebConsoleActor, and only support the ConsoleMessageWatcher codepath.
    targetActor._consoleActor.onConsoleAPICall(message);
  }

  return undefined;
}

module.exports.logEvent = logEvent;
