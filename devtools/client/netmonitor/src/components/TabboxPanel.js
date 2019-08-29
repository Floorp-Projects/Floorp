/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { L10N } = require("../utils/l10n");
const { PANELS } = require("../constants");

// Components
const Tabbar = createFactory(
  require("devtools/client/shared/components/tabs/TabBar")
);
const TabPanel = createFactory(
  require("devtools/client/shared/components/tabs/Tabs").TabPanel
);
const CookiesPanel = createFactory(require("./CookiesPanel"));
const HeadersPanel = createFactory(require("./HeadersPanel"));
const WebSocketsPanel = createFactory(require("./websockets/WebSocketsPanel"));
const ParamsPanel = createFactory(require("./ParamsPanel"));
const CachePanel = createFactory(require("./CachePanel"));
const ResponsePanel = createFactory(require("./ResponsePanel"));
const SecurityPanel = createFactory(require("./SecurityPanel"));
const StackTracePanel = createFactory(require("./StackTracePanel"));
const TimingsPanel = createFactory(require("./TimingsPanel"));

const COLLAPSE_DETAILS_PANE = L10N.getStr("collapseDetailsPane");
const CACHE_TITLE = L10N.getStr("netmonitor.tab.cache");
const COOKIES_TITLE = L10N.getStr("netmonitor.tab.cookies");
const HEADERS_TITLE = L10N.getStr("netmonitor.tab.headers");
const MESSAGES_TITLE = L10N.getStr("netmonitor.tab.messages");
const PARAMS_TITLE = L10N.getStr("netmonitor.tab.params");
const RESPONSE_TITLE = L10N.getStr("netmonitor.tab.response");
const SECURITY_TITLE = L10N.getStr("netmonitor.tab.security");
const STACK_TRACE_TITLE = L10N.getStr("netmonitor.tab.stackTrace");
const TIMINGS_TITLE = L10N.getStr("netmonitor.tab.timings");

/**
 * Tabbox panel component
 * Display the network request details
 */
class TabboxPanel extends Component {
  static get propTypes() {
    return {
      activeTabId: PropTypes.string,
      cloneSelectedRequest: PropTypes.func,
      connector: PropTypes.object.isRequired,
      openLink: PropTypes.func,
      request: PropTypes.object,
      selectTab: PropTypes.func.isRequired,
      sourceMapService: PropTypes.object,
      hideToggleButton: PropTypes.bool,
      toggleNetworkDetails: PropTypes.func,
      openNetworkDetails: PropTypes.func.isRequired,
      showWebSocketsTab: PropTypes.bool,
    };
  }

  componentDidMount() {
    this.closeOnEscRef = this.closeOnEsc.bind(this);
    window.addEventListener("keydown", this.closeOnEscRef);
  }

  componentWillUnmount() {
    window.removeEventListener("keydown", this.closeOnEscRef);
  }

  closeOnEsc(event) {
    if (event.key == "Escape") {
      event.preventDefault();
      this.props.openNetworkDetails(false);
    }
  }

  render() {
    const {
      activeTabId,
      cloneSelectedRequest = () => {},
      connector,
      hideToggleButton,
      openLink,
      request,
      selectTab,
      sourceMapService,
      toggleNetworkDetails,
      showWebSocketsTab,
    } = this.props;

    if (!request) {
      return null;
    }

    const showWebSocketsPanel =
      request.cause.type === "websocket" &&
      Services.prefs.getBoolPref("devtools.netmonitor.features.webSockets") &&
      showWebSocketsTab === undefined
        ? true
        : showWebSocketsTab;

    return Tabbar(
      {
        activeTabId,
        menuDocument: window.parent.document,
        onSelect: selectTab,
        renderOnlySelected: true,
        showAllTabsMenu: true,
        sidebarToggleButton: hideToggleButton
          ? null
          : {
              collapsed: false,
              collapsePaneTitle: COLLAPSE_DETAILS_PANE,
              expandPaneTitle: "",
              onClick: toggleNetworkDetails,
            },
      },
      TabPanel(
        {
          id: PANELS.HEADERS,
          title: HEADERS_TITLE,
          className: "panel-with-code",
        },
        HeadersPanel({
          cloneSelectedRequest,
          connector,
          openLink,
          request,
        })
      ),
      showWebSocketsPanel &&
        TabPanel(
          {
            id: PANELS.MESSAGES,
            title: MESSAGES_TITLE,
            className: "panel-with-code",
          },
          WebSocketsPanel({
            connector,
          })
        ),
      TabPanel(
        {
          id: PANELS.COOKIES,
          title: COOKIES_TITLE,
          className: "panel-with-code",
        },
        CookiesPanel({
          connector,
          openLink,
          request,
        })
      ),
      TabPanel(
        {
          id: PANELS.PARAMS,
          title: PARAMS_TITLE,
          className: "panel-with-code",
        },
        ParamsPanel({ connector, openLink, request })
      ),
      TabPanel(
        {
          id: PANELS.RESPONSE,
          title: RESPONSE_TITLE,
          className: "panel-with-code",
        },
        ResponsePanel({ request, openLink, connector })
      ),
      (request.fromCache || request.status == "304") &&
        TabPanel(
          {
            id: PANELS.CACHE,
            title: CACHE_TITLE,
          },
          CachePanel({ request, openLink, connector })
        ),
      TabPanel(
        {
          id: PANELS.TIMINGS,
          title: TIMINGS_TITLE,
        },
        TimingsPanel({
          connector,
          request,
        })
      ),
      request.cause &&
        request.cause.stacktraceAvailable &&
        TabPanel(
          {
            id: PANELS.STACK_TRACE,
            title: STACK_TRACE_TITLE,
            className: "panel-with-code",
          },
          StackTracePanel({ connector, openLink, request, sourceMapService })
        ),
      request.securityState &&
        request.securityState !== "insecure" &&
        TabPanel(
          {
            id: PANELS.SECURITY,
            title: SECURITY_TITLE,
          },
          SecurityPanel({
            connector,
            openLink,
            request,
          })
        )
    );
  }
}

module.exports = TabboxPanel;
