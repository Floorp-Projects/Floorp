/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals document */

"use strict";

const {
  createClass,
  createFactory,
  DOM,
  PropTypes,
} = require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const Actions = require("../../actions/index");
const { getSelectedRequest } = require("../../selectors/index");
const { Prefs } = require("../../prefs");

// Components
const CustomRequestPanel = createFactory(require("./custom-request-panel"));
const TabboxPanel = createFactory(require("./tabbox-panel"));

const { div } = DOM;
/*
 * Network details panel component
 */
const NetworkDetailsPanel = createClass({
  displayName: "NetworkDetailsPanel",

  propTypes: {
    activeTabId: PropTypes.string,
    cloneSelectedRequest: PropTypes.func.isRequired,
    open: PropTypes.bool,
    request: PropTypes.object,
    selectTab: PropTypes.func.isRequired,
    toolbox: PropTypes.object.isRequired,
  },

  componentDidMount() {
    // FIXME: Workaround should be removed in bug 1309183
    document.getElementById("splitter-adjustable-box")
      .setAttribute("width", Prefs.networkDetailsWidth);
    document.getElementById("splitter-adjustable-box")
      .setAttribute("height", Prefs.networkDetailsHeight);
  },

  componentWillUnmount() {
    // FIXME: Workaround should be removed in bug 1309183
    Prefs.networkDetailsWidth =
      document.getElementById("splitter-adjustable-box").getAttribute("width");
    Prefs.networkDetailsHeight =
      document.getElementById("splitter-adjustable-box").getAttribute("height");
  },

  render() {
    let {
      activeTabId,
      cloneSelectedRequest,
      open,
      request,
      selectTab,
      toolbox,
    } = this.props;

    if (!open || !request) {
      // FIXME: Workaround should be removed in bug 1309183
      document.getElementById("splitter-adjustable-box").setAttribute("hidden", true);
      return null;
    }
    // FIXME: Workaround should be removed in bug 1309183
    document.getElementById("splitter-adjustable-box").removeAttribute("hidden");

    return (
      div({ className: "network-details-panel" },
        !request.isCustom ?
          TabboxPanel({
            activeTabId,
            request,
            selectTab,
            toolbox,
          }) :
          CustomRequestPanel({
            cloneSelectedRequest,
            request,
          })
      )
    );
  }
});

module.exports = connect(
  (state) => ({
    activeTabId: state.ui.detailsPanelSelectedTab,
    open: state.ui.networkDetailsOpen,
    request: getSelectedRequest(state),
  }),
  (dispatch) => ({
    cloneSelectedRequest: () => dispatch(Actions.cloneSelectedRequest()),
    selectTab: (tabId) => dispatch(Actions.selectDetailsPanelTab(tabId)),
  }),
)(NetworkDetailsPanel);
