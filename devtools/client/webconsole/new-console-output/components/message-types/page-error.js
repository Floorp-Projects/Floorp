/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// React & Redux
const {
  createFactory,
  DOM: dom,
  PropTypes
} = require("devtools/client/shared/vendor/react");
const MessageRepeat = createFactory(require("devtools/client/webconsole/new-console-output/components/message-repeat").MessageRepeat);
const MessageIcon = createFactory(require("devtools/client/webconsole/new-console-output/components/message-icon").MessageIcon);

PageError.displayName = "PageError";

PageError.propTypes = {
  message: PropTypes.object.isRequired,
};

function PageError(props) {
  const { message } = props;
  const {category, severity} = message;

  const repeat = MessageRepeat({repeat: message.repeat});
  const icon = MessageIcon({severity: severity});

  const classes = ["message"];

  if (category) {
    classes.push(category);
  }

  if (severity) {
    classes.push(severity);
  }

  return dom.div({
    className: classes.join(" ")
  },
    icon,
    dom.span(
      {className: "message-body-wrapper message-body devtools-monospace"},
      dom.span({},
        message.messageText
      )
    ),
    repeat
  );
}

module.exports.PageError = PageError;
