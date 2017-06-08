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
  serviceContainer: PropTypes.shape({
    openNetworkPanel: PropTypes.func.isRequired,
  }),
  indent: PropTypes.number.isRequired,
  timestampsVisible: PropTypes.bool.isRequired,
  networkMessageUpdate: PropTypes.object.isRequired,
};

NetworkEventMessage.defaultProps = {
  indent: 0,
};

function NetworkEventMessage({
  indent,
  message = {},
  serviceContainer,
  timestampsVisible,
  networkMessageUpdate = {},
}) {
  const {
    actor,
    source,
    type,
    level,
    request,
    isXHR,
    timeStamp,
  } = message;

  const {
    response = {},
    totalTime,
  } = networkMessageUpdate;

  const {
    httpVersion,
    status,
    statusText,
  } = response;

  const topLevelClasses = [ "cm-s-mozilla" ];
  let statusInfo;

  if (httpVersion && status && statusText !== undefined && totalTime !== undefined) {
    statusInfo = `[${httpVersion} ${status} ${statusText} ${totalTime}ms]`;
  }

  const openNetworkMonitor = serviceContainer.openNetworkPanel
    ? () => serviceContainer.openNetworkPanel(actor)
    : null;

  const method = dom.span({className: "method" }, request.method);
  const xhr = isXHR
    ? dom.span({ className: "xhr" }, l10n.getStr("webConsoleXhrIndicator"))
    : null;
  const url = dom.a({ className: "url", title: request.url, onClick: openNetworkMonitor },
    request.url.replace(/\?.+/, ""));
  const statusBody = statusInfo
    ? dom.a({ className: "status", onClick: openNetworkMonitor }, statusInfo)
    : null;

  const messageBody = [method, xhr, url, statusBody];

  return Message({
    source,
    type,
    level,
    indent,
    topLevelClasses,
    timeStamp,
    messageBody,
    serviceContainer,
    request,
    timestampsVisible,
  });
}

module.exports = NetworkEventMessage;
