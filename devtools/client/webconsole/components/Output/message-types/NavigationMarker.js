/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// React & Redux
const {
  createFactory,
} = require("resource://devtools/client/shared/vendor/react.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

const Message = createFactory(
  require("resource://devtools/client/webconsole/components/Output/Message.js")
);

NavigationMarker.displayName = "NavigationMarker";

NavigationMarker.propTypes = {
  dispatch: PropTypes.func.isRequired,
  message: PropTypes.object.isRequired,
  serviceContainer: PropTypes.object.isRequired,
  timestampsVisible: PropTypes.bool.isRequired,
  maybeScrollToBottom: PropTypes.func,
};

function NavigationMarker(props) {
  const {
    dispatch,
    message,
    serviceContainer,
    timestampsVisible,
    maybeScrollToBottom,
  } = props;
  const {
    id: messageId,
    indent,
    source,
    type,
    level,
    timeStamp,
    messageText,
  } = message;

  return Message({
    messageId,
    source,
    type,
    level,
    messageBody: messageText,
    serviceContainer,
    dispatch,
    indent,
    timeStamp,
    timestampsVisible,
    topLevelClasses: [],
    message,
    maybeScrollToBottom,
  });
}

module.exports = NavigationMarker;
