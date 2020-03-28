/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// React & Redux
const { Component } = require("devtools/client/shared/vendor/react");
loader.lazyRequireGetter(
  this,
  "PropTypes",
  "devtools/client/shared/vendor/react-prop-types"
);
loader.lazyRequireGetter(
  this,
  "isWarningGroup",
  "devtools/client/webconsole/utils/messages",
  true
);

const {
  MESSAGE_SOURCE,
  MESSAGE_TYPE,
} = require("devtools/client/webconsole/constants");

const componentMap = new Map([
  [
    "ConsoleApiCall",
    require("devtools/client/webconsole/components/Output/message-types/ConsoleApiCall"),
  ],
  [
    "ConsoleCommand",
    require("devtools/client/webconsole/components/Output/message-types/ConsoleCommand"),
  ],
  [
    "CSSWarning",
    require("devtools/client/webconsole/components/Output/message-types/CSSWarning"),
  ],
  [
    "DefaultRenderer",
    require("devtools/client/webconsole/components/Output/message-types/DefaultRenderer"),
  ],
  [
    "EvaluationResult",
    require("devtools/client/webconsole/components/Output/message-types/EvaluationResult"),
  ],
  [
    "NetworkEventMessage",
    require("devtools/client/webconsole/components/Output/message-types/NetworkEventMessage"),
  ],
  [
    "PageError",
    require("devtools/client/webconsole/components/Output/message-types/PageError"),
  ],
  [
    "WarningGroup",
    require("devtools/client/webconsole/components/Output/message-types/WarningGroup"),
  ],
]);

class MessageContainer extends Component {
  static get propTypes() {
    return {
      messageId: PropTypes.string.isRequired,
      open: PropTypes.bool.isRequired,
      serviceContainer: PropTypes.object.isRequired,
      payload: PropTypes.object,
      timestampsVisible: PropTypes.bool.isRequired,
      repeat: PropTypes.number,
      badge: PropTypes.number,
      indent: PropTypes.number,
      networkMessageUpdate: PropTypes.object,
      getMessage: PropTypes.func.isRequired,
      inWarningGroup: PropTypes.bool,
    };
  }

  static get defaultProps() {
    return {
      open: false,
    };
  }

  shouldComponentUpdate(nextProps) {
    const triggeringUpdateProps = [
      "repeat",
      "open",
      "payload",
      "timestampsVisible",
      "networkMessageUpdate",
      "badge",
      "inWarningGroup",
    ];

    return triggeringUpdateProps.some(
      prop => this.props[prop] !== nextProps[prop]
    );
  }

  render() {
    const message = this.props.getMessage();

    const MessageComponent = getMessageComponent(message);
    return MessageComponent(Object.assign({ message }, this.props));
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
      return componentMap.get("CSSWarning");
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
    case MESSAGE_SOURCE.CONSOLE_FRONTEND:
      if (isWarningGroup(message)) {
        return componentMap.get("WarningGroup");
      }
      break;
  }

  return componentMap.get("DefaultRenderer");
}

module.exports.MessageContainer = MessageContainer;

// Exported so we can test it with unit tests.
module.exports.getMessageComponent = getMessageComponent;
