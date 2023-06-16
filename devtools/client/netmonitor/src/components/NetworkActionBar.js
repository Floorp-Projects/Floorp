/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
  createFactory,
} = require("resource://devtools/client/shared/vendor/react.js");
const {
  connect,
} = require("resource://devtools/client/shared/redux/visibility-handler-connect.js");
const {
  div,
} = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const {
  L10N,
} = require("resource://devtools/client/netmonitor/src/utils/l10n.js");
const Actions = require("resource://devtools/client/netmonitor/src/actions/index.js");
const {
  PANELS,
} = require("resource://devtools/client/netmonitor/src/constants.js");

const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

const Tabbar = createFactory(
  require("resource://devtools/client/shared/components/tabs/TabBar.js")
);
const TabPanel = createFactory(
  require("resource://devtools/client/shared/components/tabs/Tabs.js").TabPanel
);

loader.lazyGetter(this, "SearchPanel", function () {
  return createFactory(
    require("resource://devtools/client/netmonitor/src/components/search/SearchPanel.js")
  );
});

loader.lazyGetter(this, "RequestBlockingPanel", function () {
  return createFactory(
    require("resource://devtools/client/netmonitor/src/components/request-blocking/RequestBlockingPanel.js")
  );
});

loader.lazyGetter(this, "HTTPCustomRequestPanel", function () {
  return createFactory(
    require("resource://devtools/client/netmonitor/src/components/new-request/HTTPCustomRequestPanel.js")
  );
});

class NetworkActionBar extends Component {
  static get propTypes() {
    return {
      connector: PropTypes.object.isRequired,
      selectedActionBarTabId: PropTypes.string,
      selectActionBarTab: PropTypes.func.isRequired,
      toggleNetworkActionBar: PropTypes.func.isRequired,
    };
  }

  render() {
    const {
      connector,
      selectedActionBarTabId,
      selectActionBarTab,
      toggleNetworkActionBar,
    } = this.props;

    // The request blocking and search panels are available behind a pref
    const showBlockingPanel = Services.prefs.getBoolPref(
      "devtools.netmonitor.features.requestBlocking"
    );
    const showSearchPanel = Services.prefs.getBoolPref(
      "devtools.netmonitor.features.search"
    );
    const showNewCustomRequestPanel = Services.prefs.getBoolPref(
      "devtools.netmonitor.features.newEditAndResend"
    );

    return div(
      { className: "network-action-bar" },
      Tabbar(
        {
          activeTabId: selectedActionBarTabId,
          onSelect: id => selectActionBarTab(id),
          sidebarToggleButton: {
            collapsed: false,
            collapsePaneTitle: L10N.getStr("collapseActionPane"),
            expandPaneTitle: "",
            onClick: toggleNetworkActionBar,
            alignRight: true,
            canVerticalSplit: false,
          },
        },
        showNewCustomRequestPanel &&
          TabPanel(
            {
              id: PANELS.HTTP_CUSTOM_REQUEST,
              title: L10N.getStr("netmonitor.actionbar.HTTPCustomRequest"),
              className: "network-action-bar-HTTP-custom-request",
            },
            HTTPCustomRequestPanel({ connector })
          ),
        showSearchPanel &&
          TabPanel(
            {
              id: PANELS.SEARCH,
              title: L10N.getStr("netmonitor.actionbar.search"),
              className: "network-action-bar-search",
            },
            SearchPanel({ connector })
          ),
        showBlockingPanel &&
          TabPanel(
            {
              id: PANELS.BLOCKING,
              title: L10N.getStr("netmonitor.actionbar.requestBlocking2"),
              className: "network-action-bar-blocked",
            },
            RequestBlockingPanel()
          )
      )
    );
  }
}

module.exports = connect(
  state => ({
    selectedActionBarTabId: state.ui.selectedActionBarTabId,
  }),
  dispatch => ({
    selectActionBarTab: id => dispatch(Actions.selectActionBarTab(id)),
    toggleNetworkActionBar: () => dispatch(Actions.toggleNetworkActionBar()),
  })
)(NetworkActionBar);
