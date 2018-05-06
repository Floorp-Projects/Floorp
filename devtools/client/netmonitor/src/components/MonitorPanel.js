/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { div } = dom;
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/redux/visibility-handler-connect");
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

loader.lazyGetter(this, "NetworkDetailsPanel", function() {
  return createFactory(require("./NetworkDetailsPanel"));
});

// MediaQueryList object responsible for switching sidebar splitter
// between landscape and portrait mode (depending on browser window size).
const MediaQueryVert = window.matchMedia("(min-width: 700px)");

// MediaQueryList object responsible for switching the toolbar
// between single and 2-rows layout (depending on browser window size).
const MediaQuerySingleRow = window.matchMedia("(min-width: 960px)");

/**
 * Monitor panel component
 * The main panel for displaying various network request information
 */
class MonitorPanel extends Component {
  static get propTypes() {
    return {
      actions: PropTypes.object.isRequired,
      connector: PropTypes.object.isRequired,
      isEmpty: PropTypes.bool.isRequired,
      networkDetailsOpen: PropTypes.bool.isRequired,
      openNetworkDetails: PropTypes.func.isRequired,
      // Callback for opening split console.
      openSplitConsole: PropTypes.func,
      onNetworkDetailsResized: PropTypes.func.isRequired,
      request: PropTypes.object,
      selectedRequestVisible: PropTypes.bool.isRequired,
      sourceMapService: PropTypes.object,
      openLink: PropTypes.func,
      updateRequest: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      isSingleRow: MediaQuerySingleRow.matches,
      isVerticalSpliter: MediaQueryVert.matches,
    };

    this.onLayoutChange = this.onLayoutChange.bind(this);
    this.onNetworkDetailsResized = this.onNetworkDetailsResized.bind(this);
  }

  componentDidMount() {
    MediaQuerySingleRow.addListener(this.onLayoutChange);
    MediaQueryVert.addListener(this.onLayoutChange);
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
    MediaQuerySingleRow.removeListener(this.onLayoutChange);
    MediaQueryVert.removeListener(this.onLayoutChange);

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
      isSingleRow: MediaQuerySingleRow.matches,
      isVerticalSpliter: MediaQueryVert.matches,
    });
  }

  onNetworkDetailsResized(width, height) {
    // Cleaning width and height parameters, because SplitBox passes ALWAYS two values,
    // while depending on orientation ONLY ONE dimension is managed by it at a time.
    let { isVerticalSpliter }  = this.state;
    return this.props.onNetworkDetailsResized(
      isVerticalSpliter ? width : null,
      isVerticalSpliter ? null : height
    );
  }

  render() {
    let {
      actions,
      connector,
      isEmpty,
      networkDetailsOpen,
      openLink,
      openSplitConsole,
      sourceMapService,
    } = this.props;

    let initialWidth = Services.prefs.getIntPref(
      "devtools.netmonitor.panes-network-details-width");
    let initialHeight = Services.prefs.getIntPref(
      "devtools.netmonitor.panes-network-details-height");

    return (
      div({ className: "monitor-panel" },
        Toolbar({
          actions,
          connector,
          openSplitConsole,
          singleRow: this.state.isSingleRow,
        }),
        SplitBox({
          className: "devtools-responsive-container",
          initialWidth: initialWidth,
          initialHeight: initialHeight,
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
          onControlledPanelResized: this.onNetworkDetailsResized,
        }),
      )
    );
  }
}

module.exports = connect(
  (state) => ({
    isEmpty: state.requests.requests.size == 0,
    networkDetailsOpen: state.ui.networkDetailsOpen,
    request: getSelectedRequest(state),
    selectedRequestVisible: isSelectedRequestVisible(state),
  }),
  (dispatch) => ({
    openNetworkDetails: (open) => dispatch(Actions.openNetworkDetails(open)),
    onNetworkDetailsResized: (width, height) => dispatch(
      Actions.resizeNetworkDetails(width, height)
    ),
    updateRequest: (id, data, batch) => dispatch(Actions.updateRequest(id, data, batch)),
  }),
)(MonitorPanel);
