/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// React & Redux
const {
  createElement,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { ELLIPSIS } = require("devtools/shared/l10n");
const Message = createFactory(
  require("devtools/client/webconsole/components/Output/Message")
);

ConsoleCommand.displayName = "ConsoleCommand";

ConsoleCommand.propTypes = {
  message: PropTypes.object.isRequired,
  timestampsVisible: PropTypes.bool.isRequired,
  serviceContainer: PropTypes.object,
  maybeScrollToBottom: PropTypes.func,
  open: PropTypes.bool,
};

ConsoleCommand.defaultProps = {
  open: false,
};

/**
 * Displays input from the console.
 */
function ConsoleCommand(props) {
  const {
    message,
    timestampsVisible,
    serviceContainer,
    maybeScrollToBottom,
    dispatch,
    open,
  } = props;

  const { indent, source, type, level, timeStamp, id: messageId } = message;

  const messageText = trimCode(message.messageText);
  const messageLines = messageText.split("\n");

  const collapsible = messageLines.length > 5;

  // Show only first 5 lines if its collapsible and closed
  const visibleMessageText =
    collapsible && !open
      ? `${messageLines.slice(0, 5).join("\n")}${ELLIPSIS}`
      : messageText;

  // This uses a Custom Element to syntax highlight when possible. If it's not
  // (no CodeMirror editor), then it will just render text.
  const messageBody = createElement(
    "syntax-highlighted",
    null,
    visibleMessageText
  );

  // Enable collapsing the code if it has multiple lines

  return Message({
    messageId,
    source,
    type,
    level,
    topLevelClasses: [],
    messageBody,
    collapsible,
    open,
    dispatch,
    serviceContainer,
    indent,
    timeStamp,
    timestampsVisible,
    maybeScrollToBottom,
    message,
  });
}

module.exports = ConsoleCommand;

/**
 * Trim user input to avoid blank lines before and after messages
 */
function trimCode(input) {
  if (typeof input !== "string") {
    return input;
  }

  // Trim on both edges if we have a single line of content
  if (input.trim().includes("\n") === false) {
    return input.trim();
  }

  // For multiline input we want to keep the indentation of the first line
  // with non-whitespace, so we can't .trim()/.trimStart().
  return input.replace(/^\s*\n/, "").trimEnd();
}
