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
const Message = createFactory(require("devtools/client/webconsole/new-console-output/components/message"));
const { l10n } = require("devtools/client/webconsole/new-console-output/utils/messages");

NetworkEventMessage.displayName = "NetworkEventMessage";

NetworkEventMessage.propTypes = {
  message: PropTypes.object.isRequired,
  openNetworkPanel: PropTypes.func.isRequired,
};

function NetworkEventMessage(props) {
  const { message, openNetworkPanel } = props;
  const { actor, source, type, level, request, isXHR } = message;

  const topLevelClasses = [ "cm-s-mozilla" ];

  function onUrlClick() {
    openNetworkPanel(actor);
  }

  const method = dom.span({className: "method" }, request.method);
  const xhr = isXHR
    ? dom.span({ className: "xhr" }, l10n.getStr("webConsoleXhrIndicator"))
    : null;
  const url = dom.a({ className: "url", title: request.url, onClick: onUrlClick },
        request.url.replace(/\?.+/, ""));

  const messageBody = dom.span({}, method, xhr, url);

  const childProps = {
    source,
    type,
    level,
    topLevelClasses,
    messageBody,
  };
  return Message(childProps);
}

module.exports = NetworkEventMessage;
