/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// React & Redux
const { Component } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const {
  MESSAGE_SOURCE,
  MESSAGE_TYPE
} = require("devtools/client/webconsole/constants");

const componentMap = new Map([
  ["ConsoleApiCall", require("./message-types/ConsoleApiCall")],
  ["ConsoleCommand", require("./message-types/ConsoleCommand")],
  ["DefaultRenderer", require("./message-types/DefaultRenderer")],
  ["EvaluationResult", require("./message-types/EvaluationResult")],
  ["NetworkEventMessage", require("./message-types/NetworkEventMessage")],
  ["PageError", require("./message-types/PageError")]
]);

class MessageContainer extends Component {
  static get propTypes() {
    return {
      messageId: PropTypes.string.isRequired,
      open: PropTypes.bool.isRequired,
      serviceContainer: PropTypes.object.isRequired,
      tableData: PropTypes.object,
      timestampsVisible: PropTypes.bool.isRequired,
      repeat: PropTypes.number,
      networkMessageUpdate: PropTypes.object,
      getMessage: PropTypes.func.isRequired,
    };
  }

  static get defaultProps() {
    return {
      open: false,
    };
  }

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
  }

  render() {
    const message = this.props.getMessage();

    const MessageComponent = getMessageComponent(message);
    return MessageComponent(Object.assign({message}, this.props));
  }
}

function getMessageComponent(message) {
  if (!message) {
    return componentMap.get("DefaultRenderer");
  }

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
