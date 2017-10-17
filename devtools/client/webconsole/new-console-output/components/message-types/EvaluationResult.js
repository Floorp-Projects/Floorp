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
const Message = createFactory(require("devtools/client/webconsole/new-console-output/components/Message"));
const GripMessageBody = require("devtools/client/webconsole/new-console-output/components/GripMessageBody");

EvaluationResult.displayName = "EvaluationResult";

EvaluationResult.propTypes = {
  dispatch: PropTypes.func.isRequired,
  message: PropTypes.object.isRequired,
  timestampsVisible: PropTypes.bool.isRequired,
  serviceContainer: PropTypes.object,
};

function EvaluationResult(props) {
  const {
    dispatch,
    message,
    serviceContainer,
    timestampsVisible,
  } = props;

  const {
    source,
    type,
    helperType,
    level,
    id: messageId,
    indent,
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
      dispatch,
      messageId,
      grip: parameters[0],
      serviceContainer,
      useQuotes: true,
      escapeWhitespace: false,
      type,
      helperType,
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
