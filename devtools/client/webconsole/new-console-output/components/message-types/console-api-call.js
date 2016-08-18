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
const StackTrace = createFactory(require("devtools/client/shared/components/stack-trace"));
const GripMessageBody = createFactory(require("devtools/client/webconsole/new-console-output/components/grip-message-body").GripMessageBody);
const MessageRepeat = createFactory(require("devtools/client/webconsole/new-console-output/components/message-repeat").MessageRepeat);
const MessageIcon = createFactory(require("devtools/client/webconsole/new-console-output/components/message-icon").MessageIcon);

ConsoleApiCall.displayName = "ConsoleApiCall";

ConsoleApiCall.propTypes = {
  message: PropTypes.object.isRequired,
  onViewSourceInDebugger: PropTypes.func.isRequired,
};

function ConsoleApiCall(props) {
  const { message, onViewSourceInDebugger } = props;
  const {source, level, stacktrace, type} = message;

  let messageBody;
  if (type === "trace") {
    messageBody = [
      dom.span({className: "cm-variable"}, "console"),
      ".",
      dom.span({className: "cm-property"}, "trace"),
      "():"
    ];
  } else if (message.parameters) {
    messageBody = message.parameters.map((grip) => GripMessageBody({grip}));
  } else {
    messageBody = message.messageText;
  }

  const icon = MessageIcon({level});
  const repeat = MessageRepeat({repeat: message.repeat});

  let attachment = "";
  if (stacktrace) {
    attachment = dom.div({className: "stacktrace devtools-monospace"},
      StackTrace({
        stacktrace: stacktrace,
        onViewSourceInDebugger: onViewSourceInDebugger
      })
    );
  }

  const classes = ["message", "cm-s-mozilla"];

  if (source) {
    classes.push(source);
  }

  if (level) {
    classes.push(level);
  }

  if (type === "trace") {
    classes.push("open");
  }

  return dom.div({
    className: classes.join(" ")
  },
    // @TODO add timestamp
    // @TODO add indent if necessary
    icon,
    dom.span({className: "message-body-wrapper"},
      dom.span({},
        dom.span({className: "message-flex-body"},
          dom.span({className: "message-body devtools-monospace"},
            messageBody
          ),
          repeat
        ),
        attachment
      )
    )
  );
}

module.exports.ConsoleApiCall = ConsoleApiCall;
