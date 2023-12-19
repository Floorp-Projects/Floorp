/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// React & Redux
const {
  Component,
} = require("resource://devtools/client/shared/vendor/react.js");
loader.lazyRequireGetter(
  this,
  "PropTypes",
  "resource://devtools/client/shared/vendor/react-prop-types.js"
);
loader.lazyRequireGetter(
  this,
  "isWarningGroup",
  "resource://devtools/client/webconsole/utils/messages.js",
  true
);

const {
  MESSAGE_SOURCE,
  MESSAGE_TYPE,
} = require("resource://devtools/client/webconsole/constants.js");

const ConsoleApiCall = require("resource://devtools/client/webconsole/components/Output/message-types/ConsoleApiCall.js");
const ConsoleCommand = require("resource://devtools/client/webconsole/components/Output/message-types/ConsoleCommand.js");
const CSSWarning = require("resource://devtools/client/webconsole/components/Output/message-types/CSSWarning.js");
const DefaultRenderer = require("resource://devtools/client/webconsole/components/Output/message-types/DefaultRenderer.js");
const EvaluationResult = require("resource://devtools/client/webconsole/components/Output/message-types/EvaluationResult.js");
const NavigationMarker = require("resource://devtools/client/webconsole/components/Output/message-types/NavigationMarker.js");
const NetworkEventMessage = require("resource://devtools/client/webconsole/components/Output/message-types/NetworkEventMessage.js");
const PageError = require("resource://devtools/client/webconsole/components/Output/message-types/PageError.js");
const SimpleTable = require("resource://devtools/client/webconsole/components/Output/message-types/SimpleTable.js");
const JSTracerTrace = require("resource://devtools/client/webconsole/components/Output/message-types/JSTracerTrace.js");
const WarningGroup = require("resource://devtools/client/webconsole/components/Output/message-types/WarningGroup.js");

class MessageContainer extends Component {
  static get propTypes() {
    return {
      messageId: PropTypes.string.isRequired,
      open: PropTypes.bool.isRequired,
      serviceContainer: PropTypes.object.isRequired,
      cssMatchingElements: PropTypes.object,
      timestampsVisible: PropTypes.bool.isRequired,
      repeat: PropTypes.number,
      badge: PropTypes.number,
      indent: PropTypes.number,
      networkMessageUpdate: PropTypes.object,
      getMessage: PropTypes.func.isRequired,
      inWarningGroup: PropTypes.bool,
      disabled: PropTypes.bool,
    };
  }

  shouldComponentUpdate(nextProps) {
    const triggeringUpdateProps = [
      "repeat",
      "open",
      "cssMatchingElements",
      "timestampsVisible",
      "networkMessageUpdate",
      "badge",
      "inWarningGroup",
      "disabled",
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
    return DefaultRenderer;
  }

  switch (message.source) {
    case MESSAGE_SOURCE.CONSOLE_API:
      return ConsoleApiCall;
    case MESSAGE_SOURCE.JSTRACER:
      return JSTracerTrace;
    case MESSAGE_SOURCE.NETWORK:
      return NetworkEventMessage;
    case MESSAGE_SOURCE.CSS:
      return CSSWarning;
    case MESSAGE_SOURCE.JAVASCRIPT:
      switch (message.type) {
        case MESSAGE_TYPE.COMMAND:
          return ConsoleCommand;
        case MESSAGE_TYPE.RESULT:
          return EvaluationResult;
        // @TODO this is probably not the right behavior, but works for now.
        // Chrome doesn't distinguish between page errors and log messages. We
        // may want to remove the PageError component and just handle errors
        // with ConsoleApiCall.
        case MESSAGE_TYPE.LOG:
          return PageError;
        default:
          return DefaultRenderer;
      }
    case MESSAGE_SOURCE.CONSOLE_FRONTEND:
      if (isWarningGroup(message)) {
        return WarningGroup;
      }
      if (message.type === MESSAGE_TYPE.SIMPLE_TABLE) {
        return SimpleTable;
      }
      if (message.type === MESSAGE_TYPE.NAVIGATION_MARKER) {
        return NavigationMarker;
      }
      break;
  }

  return DefaultRenderer;
}

module.exports.MessageContainer = MessageContainer;

// Exported so we can test it with unit tests.
module.exports.getMessageComponent = getMessageComponent;
