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

PageError.displayName = "PageError";

PageError.propTypes = {
  message: PropTypes.object.isRequired,
  open: PropTypes.bool,
};

PageError.defaultProps = {
  open: false
};

function PageError(props) {
  const {
    message,
    open,
    sourceMapService,
    onViewSourceInDebugger,
    emitNewMessage,
  } = props;
  const {
    id: messageId,
    source,
    type,
    level,
    messageText: messageBody,
    repeat,
    stacktrace,
    frame
  } = message;

  const childProps = {
    messageId,
    open,
    source,
    type,
    level,
    topLevelClasses: [],
    messageBody,
    repeat,
    frame,
    stacktrace,
    onViewSourceInDebugger,
    sourceMapService,
    emitNewMessage,
  };
  return Message(childProps);
}

module.exports = PageError;
