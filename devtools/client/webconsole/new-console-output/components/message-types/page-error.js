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
  const repeat = MessageRepeat({repeat: message.repeat});
  const icon = MessageIcon({severity: message.severity});

  // @TODO Use of "is" is a temporary hack to get the category and severity
  // attributes to be applied. There are targeted in webconsole's CSS rules,
  // so if we remove this hack, we have to modify the CSS rules accordingly.
  return dom.div({
    class: "message",
    is: "fdt-message",
    category: message.category,
    severity: message.severity
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
