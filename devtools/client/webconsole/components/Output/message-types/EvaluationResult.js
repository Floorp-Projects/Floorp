/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// React & Redux
const { createFactory } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const Message = createFactory(
  require("devtools/client/webconsole/components/Output/Message")
);
const GripMessageBody = require("devtools/client/webconsole/components/Output/GripMessageBody");

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
  } = props;

  const {
    source,
    type,
    helperType,
    level,
    id: messageId,
    indent,
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
    messageBody = GripMessageBody({
      dispatch,
      messageId,
      grip: parameters[0],
      serviceContainer,
      useQuotes: true,
      escapeWhitespace: false,
      type,
      helperType,
      maybeScrollToBottom,
      customFormat: true,
    });
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
