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
const CollapseButton = createFactory(require("devtools/client/webconsole/new-console-output/components/collapse-button").CollapseButton);
const { l10n } = require("devtools/client/webconsole/new-console-output/utils/messages");
const actions = require("devtools/client/webconsole/new-console-output/actions/index");

NetworkEventMessage.displayName = "NetworkEventMessage";

NetworkEventMessage.propTypes = {
  message: PropTypes.object.isRequired,
  openNetworkPanel: PropTypes.func.isRequired,
  // @TODO: openLink will be used for mixed-content handling
  openLink: PropTypes.func.isRequired,
  open: PropTypes.bool.isRequired,
};

function NetworkEventMessage(props) {
  const { dispatch, message, openNetworkPanel, open } = props;
  const { actor, source, type, level, request, response, isXHR, totalTime } = message;
  let { method, url } = request;
  let { httpVersion, status, statusText } = response;

  let classes = ["message", "cm-s-mozilla"];

  classes.push(source);
  classes.push(type);
  classes.push(level);

  if (open) {
    classes.push("open");
  }

  let statusInfo = "[]";

  // @TODO: Status will be enabled after NetworkUpdateEvent packet arrives
  if (httpVersion && status && statusText && totalTime) {
    statusInfo = `[${httpVersion} ${status} ${statusText} ${totalTime}ms]`;
  }

  let xhr = l10n.getStr("webConsoleXhrIndicator");

  function onUrlClick() {
    openNetworkPanel(actor);
  }

  return dom.div({ className: classes.join(" ") },
    // @TODO add timestamp
    // @TODO add indent if necessary
    MessageIcon({ level }),
    CollapseButton({
      open,
      title: l10n.getStr("messageToggleDetails"),
      onClick: () => {
        if (open) {
          dispatch(actions.messageClose(message.id));
        } else {
          dispatch(actions.messageOpen(message.id));
        }
      },
    }),
    dom.span({
      className: "message-body-wrapper message-body devtools-monospace",
      "aria-haspopup": "true"
    },
      dom.span({ className: "method" }, method),
      isXHR ? dom.span({ className: "xhr" }, xhr) : null,
      dom.a({ className: "url", title: url, onClick: onUrlClick },
        url.replace(/\?.+/, "")),
      dom.a({ className: "status" }, statusInfo)
    )
  );
}

module.exports.NetworkEventMessage = NetworkEventMessage;
