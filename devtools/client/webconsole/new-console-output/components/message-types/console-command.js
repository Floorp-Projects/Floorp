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
const MessageIcon = createFactory(require("devtools/client/webconsole/new-console-output/components/message-icon").MessageIcon);

ConsoleCommand.displayName = "ConsoleCommand";

ConsoleCommand.propTypes = {
  message: PropTypes.object.isRequired,
};

/**
 * Displays input from the console.
 */
function ConsoleCommand(props) {
  const { message } = props;
  const {source, type, level} = message;

  const icon = MessageIcon({level});

  const classes = ["message"];

  classes.push(source);
  classes.push(type);
  classes.push(level);

  return dom.div({
    className: classes.join(" "),
    ariaLive: "off",
  },
    // @TODO add timestamp
    // @TODO add indent if necessary
    icon,
    dom.span({ className: "message-body-wrapper" },
      dom.span({ className: "message-flex-body" },
        dom.span({ className: "message-body devtools-monospace" },
          message.messageText
        )
      )
    )
  );
}

module.exports.ConsoleCommand = ConsoleCommand;
