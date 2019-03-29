/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// React & Redux
const { createFactory } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const Message = createFactory(require("devtools/client/webconsole/components/Message"));

WarningGroup.displayName = "WarningGroup";

WarningGroup.propTypes = {
  dispatch: PropTypes.func.isRequired,
  message: PropTypes.object.isRequired,
  timestampsVisible: PropTypes.bool.isRequired,
  serviceContainer: PropTypes.object,
  badge: PropTypes.number.isRequired,
};

function WarningGroup(props) {
  const {
    dispatch,
    message,
    serviceContainer,
    timestampsVisible,
    badge,
    open,
  } = props;

  const {
    source,
    type,
    level,
    id: messageId,
    indent,
    timeStamp,
  } = message;

  const messageBody = [
    message.messageText,
    " ",
    dom.span({
      className: "warning-group-badge",
      title: `${badge} messages`,
    }, badge),
  ];
  const topLevelClasses = ["cm-s-mozilla"];

  return Message({
    badge,
    collapsible: true,
    dispatch,
    indent,
    level,
    messageBody,
    messageId,
    open,
    serviceContainer,
    source,
    timeStamp,
    timestampsVisible,
    topLevelClasses,
    type,
  });
}

module.exports = WarningGroup;
