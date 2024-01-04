/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
} = require("resource://devtools/client/shared/vendor/react.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const {
  connect,
} = require("resource://devtools/client/shared/redux/visibility-handler-connect.js");
const Actions = require("resource://devtools/client/netmonitor/src/actions/index.js");
const {
  getSelectedRequest,
} = require("resource://devtools/client/netmonitor/src/selectors/index.js");

// Components
loader.lazyGetter(this, "CustomRequestPanel", function () {
  return createFactory(
    require("resource://devtools/client/netmonitor/src/components/CustomRequestPanel.js")
  );
});
loader.lazyGetter(this, "TabboxPanel", function () {
  return createFactory(
    require("resource://devtools/client/netmonitor/src/components/TabboxPanel.js")
  );
});

const { div } = dom;

/**
 * Network details panel component
 */
function NetworkDetailsBar({
  connector,
  activeTabId,
  cloneSelectedRequest,
  request,
  selectTab,
  sourceMapURLService,
  toggleNetworkDetails,
  openNetworkDetails,
  openLink,
  targetSearchResult,
}) {
  if (!request) {
    return null;
  }

  const newEditAndResendPref = Services.prefs.getBoolPref(
    "devtools.netmonitor.features.newEditAndResend"
  );

  return div(
    { className: "network-details-bar" },
    request.isCustom && !newEditAndResendPref
      ? CustomRequestPanel({
          connector,
          request,
        })
      : TabboxPanel({
          activeTabId,
          cloneSelectedRequest,
          connector,
          openLink,
          request,
          selectTab,
          sourceMapURLService,
          toggleNetworkDetails,
          openNetworkDetails,
          targetSearchResult,
        })
  );
}

NetworkDetailsBar.displayName = "NetworkDetailsBar";

NetworkDetailsBar.propTypes = {
  connector: PropTypes.object.isRequired,
  activeTabId: PropTypes.string,
  cloneSelectedRequest: PropTypes.func.isRequired,
  open: PropTypes.bool,
  request: PropTypes.object,
  selectTab: PropTypes.func.isRequired,
  sourceMapURLService: PropTypes.object,
  toggleNetworkDetails: PropTypes.func.isRequired,
  openLink: PropTypes.func,
  openNetworkDetails: PropTypes.func,
  targetSearchResult: PropTypes.object,
};

module.exports = connect(
  state => ({
    activeTabId: state.ui.detailsPanelSelectedTab,
    request: getSelectedRequest(state),
    targetSearchResult: state.search.targetSearchResult,
  }),
  dispatch => ({
    cloneSelectedRequest: () => dispatch(Actions.cloneSelectedRequest()),
    selectTab: tabId => dispatch(Actions.selectDetailsPanelTab(tabId)),
    toggleNetworkDetails: () => dispatch(Actions.toggleNetworkDetails()),
    openNetworkDetails: open => dispatch(Actions.openNetworkDetails(open)),
  })
)(NetworkDetailsBar);
