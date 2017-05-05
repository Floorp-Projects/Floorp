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

ConsoleCommand.displayName = "ConsoleCommand";

ConsoleCommand.propTypes = {
  message: PropTypes.object.isRequired,
  autoscroll: PropTypes.bool.isRequired,
  indent: PropTypes.number.isRequired,
  timestampsVisible: PropTypes.bool.isRequired,
};

ConsoleCommand.defaultProps = {
  indent: 0,
};

/**
 * Displays input from the console.
 */
function ConsoleCommand(props) {
  const {
    autoscroll,
    indent,
    message,
    timestampsVisible,
  } = props;

  const {
    source,
    type,
    level,
    messageText: messageBody,
  } = message;

  const {
    serviceContainer,
  } = props;

  return Message({
    source,
    type,
    level,
    topLevelClasses: [],
    messageBody,
    scrollToMessage: autoscroll,
    serviceContainer,
    indent,
    timestampsVisible,
  });
}

module.exports = ConsoleCommand;
