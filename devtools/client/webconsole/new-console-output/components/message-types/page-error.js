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
const StackTrace = createFactory(require("devtools/client/shared/components/stack-trace"));
const CollapseButton = createFactory(require("devtools/client/webconsole/new-console-output/components/collapse-button").CollapseButton);
const MessageRepeat = createFactory(require("devtools/client/webconsole/new-console-output/components/message-repeat").MessageRepeat);
const MessageIcon = createFactory(require("devtools/client/webconsole/new-console-output/components/message-icon").MessageIcon);

const actions = require("devtools/client/webconsole/new-console-output/actions/messages");

PageError.displayName = "PageError";

PageError.propTypes = {
  message: PropTypes.object.isRequired,
  open: PropTypes.bool,
};

PageError.defaultProps = {
  open: false
};

function PageError(props) {
  const { dispatch, message, open, sourceMapService, onViewSourceInDebugger } = props;
  const { source, level, stacktrace, frame } = message;

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


  let collapse = "";
  let attachment = "";
  if (stacktrace) {
    if (open) {
      attachment = dom.div({ className: "stacktrace devtools-monospace" },
        StackTrace({
          stacktrace: stacktrace,
          onViewSourceInDebugger: onViewSourceInDebugger
        })
      );
    }

    collapse = CollapseButton({
      open,
      onClick: function () {
        if (open) {
          dispatch(actions.messageClose(message.id));
        } else {
          dispatch(actions.messageOpen(message.id));
        }
      },
    });
  }

  const classes = ["message"];

  if (source) {
    classes.push(source);
  }

  if (level) {
    classes.push(level);
  }

  if (open === true) {
    classes.push("open");
  }

  return dom.div({
    className: classes.join(" ")
  },
    icon,
    collapse,
    dom.span({ className: "message-body-wrapper" },
      dom.span({ className: "message-flex-body" },
        dom.span({ className: "message-body devtools-monospace" },
          message.messageText
        ),
        repeat
      ),
      attachment
    )
  );
}

module.exports.PageError = PageError;
