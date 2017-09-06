/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  DOM,
  PropTypes,
} = require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const Actions = require("../actions/index");
const { getSelectedRequest } = require("../selectors/index");

// Components
const CustomRequestPanel = createFactory(require("./custom-request-panel"));
const TabboxPanel = createFactory(require("./tabbox-panel"));

const { div } = DOM;

/*
 * Network details panel component
 */
function NetworkDetailsPanel({
  activeTabId,
  cloneSelectedRequest,
  request,
  selectTab,
  sourceMapService,
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
          request,
          selectTab,
          sourceMapService,
          openLink,
        }) :
        CustomRequestPanel({
          request,
        })
    )
  );
}

NetworkDetailsPanel.displayName = "NetworkDetailsPanel";

NetworkDetailsPanel.propTypes = {
  activeTabId: PropTypes.string,
  cloneSelectedRequest: PropTypes.func.isRequired,
  open: PropTypes.bool,
  request: PropTypes.object,
  selectTab: PropTypes.func.isRequired,
  // Service to enable the source map feature.
  sourceMapService: PropTypes.object,
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
  }),
)(NetworkDetailsPanel);
