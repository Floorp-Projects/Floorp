/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// React & Redux
const {
  createClass,
  PropTypes
} = require("devtools/client/shared/vendor/react");

const {
  MESSAGE_SOURCE,
  MESSAGE_TYPE
} = require("devtools/client/webconsole/new-console-output/constants");

const componentMap = new Map([
  ["ConsoleApiCall", require("./message-types/console-api-call")],
  ["ConsoleCommand", require("./message-types/console-command")],
  ["DefaultRenderer", require("./message-types/default-renderer")],
  ["EvaluationResult", require("./message-types/evaluation-result")],
  ["NetworkEventMessage", require("./message-types/network-event-message")],
  ["PageError", require("./message-types/page-error")]
]);

const MessageContainer = createClass({
  displayName: "MessageContainer",

  propTypes: {
    open: PropTypes.bool.isRequired,
    serviceContainer: PropTypes.object.isRequired,
    tableData: PropTypes.object,
    timestampsVisible: PropTypes.bool.isRequired,
    repeat: PropTypes.number,
    networkMessageUpdate: PropTypes.object,
    getMessage: PropTypes.func.isRequired,
  },

  getDefaultProps: function () {
    return {
      open: false,
    };
  },

  shouldComponentUpdate(nextProps, nextState) {
    const repeatChanged = this.props.repeat !== nextProps.repeat;
    const openChanged = this.props.open !== nextProps.open;
    const tableDataChanged = this.props.tableData !== nextProps.tableData;
    const timestampVisibleChanged =
      this.props.timestampsVisible !== nextProps.timestampsVisible;
    const networkMessageUpdateChanged =
      this.props.networkMessageUpdate !== nextProps.networkMessageUpdate;

    return repeatChanged
      || openChanged
      || tableDataChanged
      || timestampVisibleChanged
      || networkMessageUpdateChanged;
  },

  render() {
    const message = this.props.getMessage();

    let MessageComponent = getMessageComponent(message);
    return MessageComponent(Object.assign({message, indent: message.indent}, this.props));
  }
});

function getMessageComponent(message) {
  switch (message.source) {
    case MESSAGE_SOURCE.CONSOLE_API:
      return componentMap.get("ConsoleApiCall");
    case MESSAGE_SOURCE.NETWORK:
      return componentMap.get("NetworkEventMessage");
    case MESSAGE_SOURCE.CSS:
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
