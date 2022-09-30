/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// React & Redux
const {
  createFactory,
} = require("resource://devtools/client/shared/vendor/react.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const Message = createFactory(
  require("resource://devtools/client/webconsole/components/Output/Message.js")
);
const GripMessageBody = require("resource://devtools/client/webconsole/components/Output/GripMessageBody.js");

EvaluationResult.displayName = "EvaluationResult";

EvaluationResult.propTypes = {
  dispatch: PropTypes.func.isRequired,
  message: PropTypes.object.isRequired,
  timestampsVisible: PropTypes.bool.isRequired,
  serviceContainer: PropTypes.object,
  maybeScrollToBottom: PropTypes.func,
  open: PropTypes.bool,
};

EvaluationResult.defaultProps = {
  open: false,
};

function EvaluationResult(props) {
  const {
    dispatch,
    message,
    serviceContainer,
    timestampsVisible,
    maybeScrollToBottom,
    open,
    setExpanded,
  } = props;

  const {
    source,
    type,
    helperType,
    level,
    id: messageId,
    indent,
    hasException,
    exceptionDocURL,
    stacktrace,
    frame,
    timeStamp,
    parameters,
    notes,
  } = message;

  let messageBody;
  if (
    typeof message.messageText !== "undefined" &&
    message.messageText !== null
  ) {
    const messageText = message.messageText?.getGrip
      ? message.messageText.getGrip()
      : message.messageText;
    if (typeof messageText === "string") {
      messageBody = messageText;
    } else if (
      typeof messageText === "object" &&
      messageText.type === "longString"
    ) {
      messageBody = `${messageText.initial}…`;
    }
  } else {
    messageBody = [];
    if (hasException) {
      messageBody.push("Uncaught ");
    }
    messageBody.push(
      GripMessageBody({
        dispatch,
        messageId,
        grip: parameters[0],
        key: "grip",
        serviceContainer,
        useQuotes: !hasException,
        escapeWhitespace: false,
        type,
        helperType,
        maybeScrollToBottom,
        setExpanded,
        customFormat: true,
      })
    );
  }

  const topLevelClasses = ["cm-s-mozilla"];

  return Message({
    dispatch,
    source,
    type,
    level,
    indent,
    topLevelClasses,
    messageBody,
    messageId,
    serviceContainer,
    exceptionDocURL,
    stacktrace,
    collapsible: Array.isArray(stacktrace),
    open,
    frame,
    timeStamp,
    parameters,
    notes,
    timestampsVisible,
    maybeScrollToBottom,
    message,
  });
}

module.exports = EvaluationResult;
