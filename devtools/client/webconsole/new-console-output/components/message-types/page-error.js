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
const FrameView = createFactory(require("devtools/client/shared/components/frame"));
const MessageRepeat = createFactory(require("devtools/client/webconsole/new-console-output/components/message-repeat").MessageRepeat);
const MessageIcon = createFactory(require("devtools/client/webconsole/new-console-output/components/message-icon").MessageIcon);

PageError.displayName = "PageError";

PageError.propTypes = {
  message: PropTypes.object.isRequired,
};

function PageError(props) {
  const { message, sourceMapService, onViewSourceInDebugger } = props;
  const { source, level, frame } = message;

  const repeat = MessageRepeat({repeat: message.repeat});
  const icon = MessageIcon({level});
  const shouldRenderFrame = frame && frame.source !== "debugger eval code";
  const location = dom.span({ className: "message-location devtools-monospace" },
    shouldRenderFrame ? FrameView({
      frame,
      onClick: onViewSourceInDebugger,
      showEmptyPathAsHost: true,
      sourceMapService
    }) : null
  );

  const classes = ["message"];

  if (source) {
    classes.push(source);
  }

  if (level) {
    classes.push(level);
  }

  return dom.div({
    className: classes.join(" ")
  },
    icon,
    dom.span({ className: "message-body-wrapper" },
      dom.span({ className: "message-flex-body" },
        dom.span({ className: "message-body devtools-monospace" },
          message.messageText
        ),
        repeat
      )
    )
  );
}

module.exports.PageError = PageError;
