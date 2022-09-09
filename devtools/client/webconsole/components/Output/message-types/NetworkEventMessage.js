/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// React & Redux
const {
  createFactory,
  createElement,
} = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const Message = createFactory(
  require("devtools/client/webconsole/components/Output/Message")
);
const actions = require("devtools/client/webconsole/actions/index");
const {
  isMessageNetworkError,
  l10n,
} = require("devtools/client/webconsole/utils/messages");

loader.lazyRequireGetter(
  this,
  "TabboxPanel",
  "devtools/client/netmonitor/src/components/TabboxPanel"
);
const {
  getHTTPStatusCodeURL,
} = require("devtools/client/netmonitor/src/utils/doc-utils");
const { getUnicodeUrl } = require("devtools/client/shared/unicode-url");
loader.lazyRequireGetter(
  this,
  "BLOCKED_REASON_MESSAGES",
  "devtools/client/netmonitor/src/constants",
  true
);

const LEARN_MORE = l10n.getStr("webConsoleMoreInfoLabel");

const isMacOS = Services.appinfo.OS === "Darwin";

NetworkEventMessage.displayName = "NetworkEventMessage";

NetworkEventMessage.propTypes = {
  message: PropTypes.object.isRequired,
  serviceContainer: PropTypes.shape({
    openNetworkPanel: PropTypes.func.isRequired,
    resendNetworkRequest: PropTypes.func.isRequired,
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
  disabled,
}) {
  const {
    id,
    indent,
    source,
    type,
    level,
    url,
    method,
    isXHR,
    timeStamp,
    blockedReason,
    httpVersion,
    status,
    statusText,
    totalTime,
  } = message;

  const topLevelClasses = ["cm-s-mozilla"];
  if (isMessageNetworkError(message)) {
    topLevelClasses.push("error");
  }

  let statusCode, statusInfo;

  if (
    httpVersion &&
    status &&
    statusText !== undefined &&
    totalTime !== undefined
  ) {
    const statusCodeDocURL = getHTTPStatusCodeURL(
      status.toString(),
      "webconsole"
    );
    statusCode = dom.span(
      {
        className: "status-code",
        "data-code": status,
        title: LEARN_MORE,
        onClick: e => {
          e.stopPropagation();
          e.preventDefault();
          serviceContainer.openLink(statusCodeDocURL, e);
        },
      },
      status
    );
    statusInfo = dom.span(
      { className: "status-info" },
      `[${httpVersion} `,
      statusCode,
      ` ${statusText} ${totalTime}ms]`
    );
  }

  if (blockedReason) {
    statusInfo = dom.span(
      { className: "status-info" },
      BLOCKED_REASON_MESSAGES[blockedReason]
    );
    topLevelClasses.push("network-message-blocked");
  }

  const onToggle = (messageId, e) => {
    const shouldOpenLink = (isMacOS && e.metaKey) || (!isMacOS && e.ctrlKey);
    if (shouldOpenLink) {
      serviceContainer.openLink(url, e);
      e.stopPropagation();
    } else if (open) {
      dispatch(actions.messageClose(messageId));
    } else {
      dispatch(actions.messageOpen(messageId));
    }
  };

  // Message body components.
  const requestMethod = dom.span({ className: "method" }, method);
  const xhr = isXHR
    ? dom.span({ className: "xhr" }, l10n.getStr("webConsoleXhrIndicator"))
    : null;
  const unicodeURL = getUnicodeUrl(url);
  const requestUrl = dom.span(
    { className: "url", title: unicodeURL },
    unicodeURL
  );
  const statusBody = statusInfo
    ? dom.a({ className: "status" }, statusInfo)
    : null;

  const messageBody = [xhr, requestMethod, requestUrl, statusBody];

  // API consumed by Net monitor UI components. Most of the method
  // are not needed in context of the Console panel (atm) and thus
  // let's just provide empty implementation.
  // Individual methods might be implemented step by step as needed.
  const connector = {
    viewSourceInDebugger: (srcUrl, line, column) => {
      serviceContainer.onViewSourceInDebugger({ url: srcUrl, line, column });
    },
    getLongString: grip => {
      return serviceContainer.getLongString(grip);
    },
    getTabTarget: () => {},
    sendHTTPRequest: () => {},
    triggerActivity: () => {},
    requestData: (requestId, dataType) => {
      return serviceContainer.requestData(requestId, dataType);
    },
  };

  // Only render the attachment if the network-event is
  // actually opened (performance optimization) and its not disabled.
  const attachment =
    open &&
    !disabled &&
    dom.div(
      {
        className: "network-info network-monitor",
      },
      createElement(TabboxPanel, {
        connector,
        activeTabId: networkMessageActiveTabId,
        request: networkMessageUpdate,
        sourceMapURLService: serviceContainer.sourceMapURLService,
        openLink: serviceContainer.openLink,
        selectTab: tabId => {
          dispatch(actions.selectNetworkMessageTab(tabId));
        },
        openNetworkDetails: enabled => {
          if (!enabled) {
            dispatch(actions.messageClose(id));
          }
        },
        hideToggleButton: true,
        showMessagesView: false,
      })
    );

  const request = { url, method };
  return Message({
    dispatch,
    messageId: id,
    source,
    type,
    level,
    indent,
    collapsible: true,
    open,
    disabled,
    onToggle,
    attachment,
    topLevelClasses,
    timeStamp,
    messageBody,
    serviceContainer,
    request,
    timestampsVisible,
    isBlockedNetworkMessage: !!blockedReason,
    message,
  });
}

module.exports = NetworkEventMessage;
