/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// React & Redux
const { createFactory } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const Message = createFactory(require("devtools/client/webconsole/components/Message"));
const GripMessageBody = require("devtools/client/webconsole/components/GripMessageBody");

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
  if (typeof message.messageText !== "undefined" && message.messageText !== null) {
    if (typeof message.messageText === "string") {
      messageBody = message.messageText;
    } else if (
      typeof message.messageText === "object"
      && message.messageText.type === "longString"
    ) {
      messageBody = `${message.messageText.initial}…`;
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
  });
}

module.exports = EvaluationResult;
