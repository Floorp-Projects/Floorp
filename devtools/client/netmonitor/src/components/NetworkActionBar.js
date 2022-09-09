/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const {
  connect,
} = require("devtools/client/shared/redux/visibility-handler-connect");
const { div } = require("devtools/client/shared/vendor/react-dom-factories");
const { L10N } = require("devtools/client/netmonitor/src/utils/l10n");
const Actions = require("devtools/client/netmonitor/src/actions/index");
const { PANELS } = require("devtools/client/netmonitor/src/constants");

const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const Tabbar = createFactory(
  require("devtools/client/shared/components/tabs/TabBar")
);
const TabPanel = createFactory(
  require("devtools/client/shared/components/tabs/Tabs").TabPanel
);

loader.lazyGetter(this, "SearchPanel", function() {
  return createFactory(
    require("devtools/client/netmonitor/src/components/search/SearchPanel")
  );
});

loader.lazyGetter(this, "RequestBlockingPanel", function() {
  return createFactory(
    require("devtools/client/netmonitor/src/components/request-blocking/RequestBlockingPanel")
  );
});

loader.lazyGetter(this, "HTTPCustomRequestPanel", function() {
  return createFactory(
    require("devtools/client/netmonitor/src/components/new-request/HTTPCustomRequestPanel")
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
