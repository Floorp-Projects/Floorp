/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// React & Redux
const {
  createClass,
  createFactory,
  PropTypes
} = require("devtools/client/shared/vendor/react");

const componentMap = new Map([
  ["ConsoleApiCall", require("./message-types/console-api-call").ConsoleApiCall],
  ["ConsoleCommand", require("./message-types/console-command").ConsoleCommand],
  ["DefaultRenderer", require("./message-types/default-renderer").DefaultRenderer],
  ["EvaluationResult", require("./message-types/evaluation-result").EvaluationResult],
  ["PageError", require("./message-types/page-error").PageError]
]);

const MessageContainer = createClass({
  displayName: "MessageContainer",

  propTypes: {
    message: PropTypes.object.isRequired
  },

  render() {
    const { message } = this.props;
    let MessageComponent = createFactory(getMessageComponent(message));
    return MessageComponent({ message });
  }
});

function getMessageComponent(message) {
  // @TODO Once packets have been converted to Chrome RDP structure, remove.
  if (message.messageType) {
    let {messageType} = message;
    if (!componentMap.has(messageType)) {
      return componentMap.get("DefaultRenderer");
    }
    return componentMap.get(messageType);
  }

  switch (message.source) {
    case "javascript":
      switch (message.type) {
        case "command":
          return componentMap.get("ConsoleCommand");
      }
      break;
  }

  return componentMap.get("DefaultRenderer");
}

module.exports.MessageContainer = MessageContainer;

// Exported so we can test it with unit tests.
module.exports.getMessageComponent = getMessageComponent;
