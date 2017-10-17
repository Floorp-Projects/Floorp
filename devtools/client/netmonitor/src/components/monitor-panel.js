/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const {
  createClass,
  createFactory,
  DOM,
  PropTypes,
} = require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const { findDOMNode } = require("devtools/client/shared/vendor/react-dom");
const Actions = require("../actions/index");
const { getFormDataSections } = require("../utils/request-utils");
const { getSelectedRequest } = require("../selectors/index");

// Components
const SplitBox = createFactory(require("devtools/client/shared/components/splitter/SplitBox"));
const NetworkDetailsPanel = createFactory(require("./network-details-panel"));
const RequestList = createFactory(require("./request-list"));
const Toolbar = createFactory(require("./toolbar"));
const { div } = DOM;
const MediaQueryList = window.matchMedia("(min-width: 700px)");

/**
 * Monitor panel component
 * The main panel for displaying various network request information
 */
const MonitorPanel = createClass({
  displayName: "MonitorPanel",

  propTypes: {
    connector: PropTypes.object.isRequired,
    isEmpty: PropTypes.bool.isRequired,
    networkDetailsOpen: PropTypes.bool.isRequired,
    openNetworkDetails: PropTypes.func.isRequired,
    request: PropTypes.object,
    sourceMapService: PropTypes.object,
    openLink: PropTypes.func,
    updateRequest: PropTypes.func.isRequired,
  },

  getInitialState() {
    return {
      isVerticalSpliter: MediaQueryList.matches,
    };
  },

  componentDidMount() {
    MediaQueryList.addListener(this.onLayoutChange);
  },

  componentWillReceiveProps(nextProps) {
    let {
      request = {},
      updateRequest,
    } = nextProps;
    let {
      formDataSections,
      requestHeaders,
      requestHeadersFromUploadStream,
      requestPostData,
    } = request;

    if (!formDataSections && requestHeaders &&
        requestHeadersFromUploadStream && requestPostData) {
      getFormDataSections(
        requestHeaders,
        requestHeadersFromUploadStream,
        requestPostData,
        this.props.connector.getLongString,
      ).then((newFormDataSections) => {
        updateRequest(
          request.id,
          { formDataSections: newFormDataSections },
          true,
        );
      });
    }
  },

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
  },

  onLayoutChange() {
    this.setState({
      isVerticalSpliter: MediaQueryList.matches,
    });
  },

  render() {
    let {
      connector,
      isEmpty,
      networkDetailsOpen,
      sourceMapService,
      openLink
    } = this.props;

    let initialWidth = Services.prefs.getIntPref(
        "devtools.netmonitor.panes-network-details-width");
    let initialHeight = Services.prefs.getIntPref(
        "devtools.netmonitor.panes-network-details-height");
    return (
      div({ className: "monitor-panel" },
        Toolbar(),
        SplitBox({
          className: "devtools-responsive-container",
          initialWidth: `${initialWidth}px`,
          initialHeight: `${initialHeight}px`,
          minSize: "50px",
          maxSize: "80%",
          splitterSize: "1px",
          startPanel: RequestList({ isEmpty, connector }),
          endPanel: networkDetailsOpen && NetworkDetailsPanel({
            ref: "endPanel",
            connector,
            sourceMapService,
            openLink,
          }),
          endPanelCollapsed: !networkDetailsOpen,
          endPanelControl: true,
          vert: this.state.isVerticalSpliter,
        }),
      )
    );
  }
});

module.exports = connect(
  (state) => ({
    isEmpty: state.requests.requests.isEmpty(),
    networkDetailsOpen: state.ui.networkDetailsOpen,
    request: getSelectedRequest(state),
  }),
  (dispatch) => ({
    openNetworkDetails: (open) => dispatch(Actions.openNetworkDetails(open)),
    updateRequest: (id, data, batch) => dispatch(Actions.updateRequest(id, data, batch)),
  }),
)(MonitorPanel);
