/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { L10N } = require("../utils/l10n");
const { PANELS } = require("../constants");

// Components
const Tabbar = createFactory(require("devtools/client/shared/components/tabs/TabBar"));
const TabPanel = createFactory(require("devtools/client/shared/components/tabs/Tabs").TabPanel);
const CookiesPanel = createFactory(require("./CookiesPanel"));
const HeadersPanel = createFactory(require("./HeadersPanel"));
const ParamsPanel = createFactory(require("./ParamsPanel"));
const ResponsePanel = createFactory(require("./ResponsePanel"));
const SecurityPanel = createFactory(require("./SecurityPanel"));
const StackTracePanel = createFactory(require("./StackTracePanel"));
const TimingsPanel = createFactory(require("./TimingsPanel"));

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
        HeadersPanel({
          cloneSelectedRequest,
          connector,
          openLink,
          request,
        }),
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
        ParamsPanel({ connector, openLink, request }),
      ),
      TabPanel({
        id: PANELS.RESPONSE,
        title: RESPONSE_TITLE,
      },
        ResponsePanel({ request, openLink, connector }),
      ),
      TabPanel({
        id: PANELS.TIMINGS,
        title: TIMINGS_TITLE,
      },
        TimingsPanel({ request }),
      ),
      request.cause && request.cause.stacktraceAvailable &&
      TabPanel({
        id: PANELS.STACK_TRACE,
        title: STACK_TRACE_TITLE,
      },
        StackTracePanel({ connector, openLink, request, sourceMapService }),
      ),
      request.securityState && request.securityState !== "insecure" &&
      TabPanel({
        id: PANELS.SECURITY,
        title: SECURITY_TITLE,
      },
        SecurityPanel({
          connector,
          openLink,
          request,
        }),
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

module.exports = TabboxPanel;
