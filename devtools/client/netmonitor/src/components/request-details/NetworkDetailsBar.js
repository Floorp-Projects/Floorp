/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const {
  connect,
} = require("devtools/client/shared/redux/visibility-handler-connect");
const Actions = require("devtools/client/netmonitor/src/actions/index");
const {
  getSelectedRequest,
} = require("devtools/client/netmonitor/src/selectors/index");

// Components
loader.lazyGetter(this, "CustomRequestPanel", function() {
  return createFactory(
    require("devtools/client/netmonitor/src/components/CustomRequestPanel")
  );
});
loader.lazyGetter(this, "TabboxPanel", function() {
  return createFactory(
    require("devtools/client/netmonitor/src/components/TabboxPanel")
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
