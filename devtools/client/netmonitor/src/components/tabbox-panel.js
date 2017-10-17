/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PropTypes,
} = require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const { L10N } = require("../utils/l10n");
const { PANELS } = require("../constants");

// Components
const Tabbar = createFactory(require("devtools/client/shared/components/tabs/TabBar"));
const TabPanel = createFactory(require("devtools/client/shared/components/tabs/Tabs").TabPanel);
const CookiesPanel = createFactory(require("./cookies-panel"));
const HeadersPanel = createFactory(require("./headers-panel"));
const ParamsPanel = createFactory(require("./params-panel"));
const ResponsePanel = createFactory(require("./response-panel"));
const SecurityPanel = createFactory(require("./security-panel"));
const StackTracePanel = createFactory(require("./stack-trace-panel"));
const TimingsPanel = createFactory(require("./timings-panel"));

const COOKIES_TITLE = L10N.getStr("netmonitor.tab.cookies");
const HEADERS_TITLE = L10N.getStr("netmonitor.tab.headers");
const PARAMS_TITLE = L10N.getStr("netmonitor.tab.params");
const RESPONSE_TITLE = L10N.getStr("netmonitor.tab.response");
const SECURITY_TITLE = L10N.getStr("netmonitor.tab.security");
const STACK_TRACE_TITLE = L10N.getStr("netmonitor.tab.stackTrace");
const TIMINGS_TITLE = L10N.getStr("netmonitor.tab.timings");

/**
 * Tabbox panel component
 * Display the network request details
 */
function TabboxPanel({
  activeTabId,
  cloneSelectedRequest = () => {},
  connector,
  openLink,
  request,
  selectTab,
  sourceMapService,
}) {
  if (!request) {
    return null;
  }

  return (
    Tabbar({
      activeTabId,
      menuDocument: window.parent.document,
      onSelect: selectTab,
      renderOnlySelected: true,
      showAllTabsMenu: true,
    },
      TabPanel({
        id: PANELS.HEADERS,
        title: HEADERS_TITLE,
      },
        HeadersPanel({ request, cloneSelectedRequest, openLink }),
      ),
      TabPanel({
        id: PANELS.COOKIES,
        title: COOKIES_TITLE,
      },
        CookiesPanel({ request, openLink }),
      ),
      TabPanel({
        id: PANELS.PARAMS,
        title: PARAMS_TITLE,
      },
        ParamsPanel({ request, openLink }),
      ),
      TabPanel({
        id: PANELS.RESPONSE,
        title: RESPONSE_TITLE,
      },
        ResponsePanel({ request, openLink }),
      ),
      TabPanel({
        id: PANELS.TIMINGS,
        title: TIMINGS_TITLE,
      },
        TimingsPanel({ request }),
      ),
      request.cause && request.cause.stacktrace && request.cause.stacktrace.length > 0 &&
      TabPanel({
        id: PANELS.STACK_TRACE,
        title: STACK_TRACE_TITLE,
      },
        StackTracePanel({ request, sourceMapService, openLink, connector }),
      ),
      request.securityState && request.securityState !== "insecure" &&
      TabPanel({
        id: PANELS.SECURITY,
        title: SECURITY_TITLE,
      },
        SecurityPanel({ request, openLink }),
      ),
    )
  );
}

TabboxPanel.displayName = "TabboxPanel";

TabboxPanel.propTypes = {
  activeTabId: PropTypes.string,
  cloneSelectedRequest: PropTypes.func,
  connector: PropTypes.object.isRequired,
  openLink: PropTypes.func,
  request: PropTypes.object,
  selectTab: PropTypes.func.isRequired,
  sourceMapService: PropTypes.object,
};

module.exports = connect()(TabboxPanel);
