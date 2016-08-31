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

const {
  MESSAGE_SOURCE,
  MESSAGE_TYPE
} = require("devtools/client/webconsole/new-console-output/constants");

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
    message: PropTypes.object.isRequired,
    sourceMapService: PropTypes.object,
    onViewSourceInDebugger: PropTypes.func.isRequired,
    open: PropTypes.bool.isRequired,
  },

  shouldComponentUpdate(nextProps, nextState) {
    return this.props.message.repeat !== nextProps.message.repeat
      || this.props.open !== nextProps.open;
  },

  render() {
    const {
      dispatch,
      message,
      sourceMapService,
      onViewSourceInDebugger,
      open
    } = this.props;

    let MessageComponent = createFactory(getMessageComponent(message));
    return MessageComponent({
      dispatch,
      message,
      sourceMapService,
      onViewSourceInDebugger,
      open
    });
  }
});

function getMessageComponent(message) {
  switch (message.source) {
    case MESSAGE_SOURCE.CONSOLE_API:
      return componentMap.get("ConsoleApiCall");
    case MESSAGE_SOURCE.JAVASCRIPT:
      switch (message.type) {
        case MESSAGE_TYPE.COMMAND:
          return componentMap.get("ConsoleCommand");
        case MESSAGE_TYPE.RESULT:
          return componentMap.get("EvaluationResult");
        // @TODO this is probably not the right behavior, but works for now.
        // Chrome doesn't distinguish between page errors and log messages. We
        // may want to remove the PageError component and just handle errors
        // with ConsoleApiCall.
        case MESSAGE_TYPE.LOG:
          return componentMap.get("PageError");
        default:
          return componentMap.get("DefaultRenderer");
      }
  }

  return componentMap.get("DefaultRenderer");
}

module.exports.MessageContainer = MessageContainer;

// Exported so we can test it with unit tests.
module.exports.getMessageComponent = getMessageComponent;
