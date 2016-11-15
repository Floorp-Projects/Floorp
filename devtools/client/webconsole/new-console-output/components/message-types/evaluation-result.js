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
const GripMessageBody = createFactory(require("devtools/client/webconsole/new-console-output/components/grip-message-body"));

EvaluationResult.displayName = "EvaluationResult";

EvaluationResult.propTypes = {
  message: PropTypes.object.isRequired,
  indent: PropTypes.number.isRequired,
};

EvaluationResult.defaultProps = {
  indent: 0,
};

function EvaluationResult(props) {
  const { message, serviceContainer, indent } = props;
  const {
    source,
    type,
    level,
    id: messageId,
    exceptionDocURL,
    frame,
  } = message;

  let messageBody;
  if (message.messageText) {
    messageBody = message.messageText;
  } else {
    messageBody = GripMessageBody({grip: message.parameters, serviceContainer});
  }

  const topLevelClasses = ["cm-s-mozilla"];

  const childProps = {
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
  };
  return Message(childProps);
}

module.exports = EvaluationResult;
