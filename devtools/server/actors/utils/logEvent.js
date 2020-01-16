/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { formatDisplayName } = require("devtools/server/actors/frame");

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
  const { sourceActor, line, column } = threadActor.sources.getFrameLocation(
    frame
  );
  const displayName = formatDisplayName(frame);
  const completion = frame.evalWithBindings(expression, {
    displayName,
    ...bindings,
  });

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

  const message = {
    filename: sourceActor.url,
    lineNumber: line,
    columnNumber: column,
    arguments: value,
    level,
  };

  threadActor._parent._consoleActor.onConsoleAPICall(message);
}

module.exports.logEvent = logEvent;
