/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// React & Redux
const {
  createElement,
  DOM: dom,
  PropTypes
} = require("devtools/client/shared/vendor/react");
const { MessageRepeat } = require("devtools/client/webconsole/new-console-output/components/message-repeat");

ConsoleApiCall.displayName = "ConsoleApiCall";

ConsoleApiCall.propTypes = {
  message: PropTypes.object.isRequired,
};

function ConsoleApiCall(props) {
  const { message } = props;
  const messageBody =
    dom.span({className: "message-body devtools-monospace"},
      formatTextContent(message.data.arguments));
  const repeat = createElement(MessageRepeat, {repeat: message.repeat});
  const children = [
    messageBody,
    repeat
  ];

  return dom.div({ className: "message cm-s-mozilla" },
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
  return args.map(function(arg, i, arr) {
    const str = dom.span({className: "console-string"}, arg);
    if (i < arr.length - 1) {
      return [str, " "];
    }
    return str;
  });
}

module.exports.ConsoleApiCall = ConsoleApiCall;
