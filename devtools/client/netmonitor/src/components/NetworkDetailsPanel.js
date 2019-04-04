/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { connect } = require("devtools/client/shared/redux/visibility-handler-connect");
const Actions = require("../actions/index");
const { getSelectedRequest } = require("../selectors/index");

// Components
loader.lazyGetter(this, "CustomRequestPanel", function() {
  return createFactory(require("./CustomRequestPanel"));
});
loader.lazyGetter(this, "TabboxPanel", function() {
  return createFactory(require("./TabboxPanel"));
});

const { div } = dom;

/**
 * Network details panel component
 */
function NetworkDetailsPanel({
  connector,
  activeTabId,
  cloneSelectedRequest,
  request,
  selectTab,
  sourceMapService,
  toggleNetworkDetails,
  openNetworkDetails,
  openLink,
}) {
  if (!request) {
    return null;
  }

  return (
    div({ className: "network-details-panel" },
      !request.isCustom ?
        TabboxPanel({
          activeTabId,
          cloneSelectedRequest,
          connector,
          openLink,
          request,
          selectTab,
          sourceMapService,
          toggleNetworkDetails,
          openNetworkDetails,
        }) :
        CustomRequestPanel({
          connector,
          request,
        })
    )
  );
}

NetworkDetailsPanel.displayName = "NetworkDetailsPanel";

NetworkDetailsPanel.propTypes = {
  connector: PropTypes.object.isRequired,
  activeTabId: PropTypes.string,
  cloneSelectedRequest: PropTypes.func.isRequired,
  open: PropTypes.bool,
  request: PropTypes.object,
  selectTab: PropTypes.func.isRequired,
  sourceMapService: PropTypes.object,
  toggleNetworkDetails: PropTypes.func.isRequired,
  openLink: PropTypes.func,
};

module.exports = connect(
  (state) => ({
    activeTabId: state.ui.detailsPanelSelectedTab,
    request: getSelectedRequest(state),
  }),
  (dispatch) => ({
    cloneSelectedRequest: () => dispatch(Actions.cloneSelectedRequest()),
    selectTab: (tabId) => dispatch(Actions.selectDetailsPanelTab(tabId)),
    toggleNetworkDetails: () => dispatch(Actions.toggleNetworkDetails()),
    openNetworkDetails: (open) => dispatch(Actions.openNetworkDetails(open)),
  }),
)(NetworkDetailsPanel);
