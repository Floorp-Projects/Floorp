/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { div } = dom;
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const { findDOMNode } = require("devtools/client/shared/vendor/react-dom");
const Actions = require("../actions/index");
const { updateFormDataSections } = require("../utils/request-utils");
const {
  getSelectedRequest,
  isSelectedRequestVisible,
} = require("../selectors/index");

// Components
const SplitBox = createFactory(require("devtools/client/shared/components/splitter/SplitBox"));
const RequestList = createFactory(require("./RequestList"));
const Toolbar = createFactory(require("./Toolbar"));

loader.lazyGetter(this, "NetworkDetailsPanel", function () {
  return createFactory(require("./NetworkDetailsPanel"));
});

const MediaQueryList = window.matchMedia("(min-width: 700px)");

/**
 * Monitor panel component
 * The main panel for displaying various network request information
 */
class MonitorPanel extends Component {
  static get propTypes() {
    return {
      connector: PropTypes.object.isRequired,
      isEmpty: PropTypes.bool.isRequired,
      networkDetailsOpen: PropTypes.bool.isRequired,
      openNetworkDetails: PropTypes.func.isRequired,
      request: PropTypes.object,
      selectedRequestVisible: PropTypes.func.isRequired,
      sourceMapService: PropTypes.object,
      openLink: PropTypes.func,
      updateRequest: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      isVerticalSpliter: MediaQueryList.matches,
    };

    this.onLayoutChange = this.onLayoutChange.bind(this);
  }

  componentDidMount() {
    MediaQueryList.addListener(this.onLayoutChange);
  }

  componentWillReceiveProps(nextProps) {
    updateFormDataSections(nextProps);
  }

  componentDidUpdate() {
    let { selectedRequestVisible, openNetworkDetails } = this.props;
    if (!selectedRequestVisible) {
      openNetworkDetails(false);
    }
  }

  componentWillUnmount() {
    MediaQueryList.removeListener(this.onLayoutChange);

    let { clientWidth, clientHeight } = findDOMNode(this.refs.endPanel) || {};

    if (this.state.isVerticalSpliter && clientWidth) {
      Services.prefs.setIntPref(
        "devtools.netmonitor.panes-network-details-width", clientWidth);
    }
    if (!this.state.isVerticalSpliter && clientHeight) {
      Services.prefs.setIntPref(
        "devtools.netmonitor.panes-network-details-height", clientHeight);
    }
  }

  onLayoutChange() {
    this.setState({
      isVerticalSpliter: MediaQueryList.matches,
    });
  }

  render() {
    let {
      connector,
      isEmpty,
      networkDetailsOpen,
      openLink,
      sourceMapService,
    } = this.props;

    let initialWidth = Services.prefs.getIntPref(
        "devtools.netmonitor.panes-network-details-width");
    let initialHeight = Services.prefs.getIntPref(
        "devtools.netmonitor.panes-network-details-height");

    return (
      div({ className: "monitor-panel" },
        Toolbar({ connector }),
        SplitBox({
          className: "devtools-responsive-container",
          initialWidth: `${initialWidth}px`,
          initialHeight: `${initialHeight}px`,
          minSize: "50px",
          maxSize: "80%",
          splitterSize: 1,
          startPanel: RequestList({ isEmpty, connector }),
          endPanel: networkDetailsOpen && NetworkDetailsPanel({
            ref: "endPanel",
            connector,
            openLink,
            sourceMapService,
          }),
          endPanelCollapsed: !networkDetailsOpen,
          endPanelControl: true,
          vert: this.state.isVerticalSpliter,
        }),
      )
    );
  }
}

module.exports = connect(
  (state) => ({
    isEmpty: state.requests.requests.isEmpty(),
    networkDetailsOpen: state.ui.networkDetailsOpen,
    request: getSelectedRequest(state),
    selectedRequestVisible: isSelectedRequestVisible(state),
  }),
  (dispatch) => ({
    openNetworkDetails: (open) => dispatch(Actions.openNetworkDetails(open)),
    updateRequest: (id, data, batch) => dispatch(Actions.updateRequest(id, data, batch)),
  }),
)(MonitorPanel);
