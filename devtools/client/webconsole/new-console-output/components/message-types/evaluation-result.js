/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// React & Redux
const {
  createFactory,
  PropTypes
} = require("devtools/client/shared/vendor/react");
const Message = createFactory(require("devtools/client/webconsole/new-console-output/components/message"));
const GripMessageBody = require("devtools/client/webconsole/new-console-output/components/grip-message-body");

EvaluationResult.displayName = "EvaluationResult";

EvaluationResult.propTypes = {
  message: PropTypes.object.isRequired,
  indent: PropTypes.number.isRequired,
  timestampsVisible: PropTypes.bool.isRequired,
};

EvaluationResult.defaultProps = {
  indent: 0,
};

function EvaluationResult(props) {
  const {
    message,
    serviceContainer,
    indent,
    timestampsVisible,
  } = props;

  const {
    source,
    type,
    level,
    id: messageId,
    exceptionDocURL,
    frame,
    timeStamp,
    parameters,
    notes,
  } = message;

  let messageBody;
  if (message.messageText) {
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
      grip: parameters,
      serviceContainer,
      useQuotes: true,
      escapeWhitespace: false,
    });
  }

  const topLevelClasses = ["cm-s-mozilla"];

  return Message({
    source,
    type,
    level,
    indent,
    topLevelClasses,
    messageBody,
    messageId,
    scrollToMessage: props.autoscroll,
    serviceContainer,
    exceptionDocURL,
    frame,
    timeStamp,
    parameters,
    notes,
    timestampsVisible,
  });
}

module.exports = EvaluationResult;
