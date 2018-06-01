/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// React & Redux
const { createFactory } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const Message = createFactory(require("devtools/client/webconsole/components/Message"));
const actions = require("devtools/client/webconsole/actions/index");
const { l10n } = require("devtools/client/webconsole/utils/messages");
const TabboxPanel = createFactory(require("devtools/client/netmonitor/src/components/TabboxPanel"));
const { getHTTPStatusCodeURL } = require("devtools/client/netmonitor/src/utils/mdn-utils");
const LEARN_MORE = l10n.getStr("webConsoleMoreInfoLabel");

NetworkEventMessage.displayName = "NetworkEventMessage";

NetworkEventMessage.propTypes = {
  message: PropTypes.object.isRequired,
  serviceContainer: PropTypes.shape({
    openNetworkPanel: PropTypes.func.isRequired,
  }),
  timestampsVisible: PropTypes.bool.isRequired,
  networkMessageUpdate: PropTypes.object.isRequired,
};

/**
 * This component is responsible for rendering network messages
 * in the Console panel.
 *
 * Network logs are expandable and the user can inspect it inline
 * within the Console panel (no need to switch to the Network panel).
 *
 * HTTP details are rendered using `TabboxPanel` component used to
 * render contents of the side bar in the Network panel.
 *
 * All HTTP details data are fetched from the backend on-demand
 * when the user is expanding network log for the first time.
 */
function NetworkEventMessage({
  message = {},
  serviceContainer,
  timestampsVisible,
  networkMessageUpdate = {},
  networkMessageActiveTabId,
  dispatch,
  open,
}) {
  const {
    id,
    indent,
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
  let statusCode, statusInfo;

  if (httpVersion && status && statusText !== undefined && totalTime !== undefined) {
    const statusCodeDocURL = getHTTPStatusCodeURL(status.toString(), "webconsole");
    statusCode = dom.span({
      className: "status-code",
      "data-code": status,
      title: LEARN_MORE,
      onClick: (e) => {
        e.stopPropagation();
        e.preventDefault();
        serviceContainer.openLink(statusCodeDocURL, e);
      }
    }, status);
    statusInfo = dom.span(
      {className: "status-info"},
      `[${httpVersion} `, statusCode, ` ${statusText} ${totalTime}ms]`
    );
  }

  const toggle = () => {
    if (open) {
      dispatch(actions.messageClose(id));
    } else {
      dispatch(actions.messageOpen(id));
    }
  };

  // Message body components.
  const method = dom.span({className: "method" }, request.method);
  const xhr = isXHR
    ? dom.span({ className: "xhr" }, l10n.getStr("webConsoleXhrIndicator"))
    : null;
  const requestUrl = dom.a({ className: "url", title: request.url, onClick: toggle },
    request.url);
  const statusBody = statusInfo
    ? dom.a({ className: "status", onClick: toggle }, statusInfo)
    : null;

  const messageBody = [xhr, method, requestUrl, statusBody];

  // API consumed by Net monitor UI components. Most of the method
  // are not needed in context of the Console panel (atm) and thus
  // let's just provide empty implementation.
  // Individual methods might be implemented step by step as needed.
  const connector = {
    viewSourceInDebugger: (url, line) => {
      serviceContainer.onViewSourceInDebugger({url, line});
    },
    getLongString: (grip) => {
      return serviceContainer.getLongString(grip);
    },
    getTabTarget: () => {},
    getNetworkRequest: () => {},
    sendHTTPRequest: () => {},
    setPreferences: () => {},
    triggerActivity: () => {},
    requestData: (requestId, dataType) => {
      return serviceContainer.requestData(requestId, dataType);
    },
  };

  // Only render the attachment if the network-event is
  // actually opened (performance optimization).
  const attachment = open && dom.div({
    className: "network-info network-monitor devtools-monospace"},
    TabboxPanel({
      connector,
      activeTabId: networkMessageActiveTabId,
      request: networkMessageUpdate,
      sourceMapService: serviceContainer.sourceMapService,
      openLink: serviceContainer.openLink,
      selectTab: (tabId) => {
        dispatch(actions.selectNetworkMessageTab(tabId));
      },
      hideToggleButton: true,
    })
  );

  return Message({
    dispatch,
    messageId: id,
    source,
    type,
    level,
    indent,
    collapsible: true,
    open,
    attachment,
    topLevelClasses,
    timeStamp,
    messageBody,
    serviceContainer,
    request,
    timestampsVisible,
  });
}

module.exports = NetworkEventMessage;
