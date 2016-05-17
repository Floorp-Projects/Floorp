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

ConsoleApiCall.displayName = "ConsoleApiCall";

ConsoleApiCall.propTypes = {
  message: PropTypes.object.isRequired,
};

function ConsoleApiCall(props) {
  const { message } = props;
  const messageBody =
    dom.span({className: "message-body devtools-monospace"},
      formatTextContent(message.data.arguments));
  const icon = MessageIcon({severity: message.severity});
  const repeat = MessageRepeat({repeat: message.repeat});
  const children = [
    messageBody,
    repeat
  ];

  // @TODO Use of "is" is a temporary hack to get the category and severity
  // attributes to be applied. There are targeted in webconsole's CSS rules,
  // so if we remove this hack, we have to modify the CSS rules accordingly.
  return dom.div({
    class: "message cm-s-mozilla",
    is: "fdt-message",
    category: message.category,
    severity: message.severity
  },
    icon,
    dom.span({className: "message-body-wrapper"},
      dom.span({},
        dom.span({className: "message-flex-body"},
          children
        )
      )
    )
  );
}

function formatTextContent(args) {
  return args.map(function (arg, i, arr) {
    const str = dom.span({className: "console-string"}, arg);
    if (i < arr.length - 1) {
      return [str, " "];
    }
    return str;
  });
}

module.exports.ConsoleApiCall = ConsoleApiCall;
