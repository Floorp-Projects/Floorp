/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createClass,
  createFactory,
  DOM,
  PropTypes,
} = require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const { findDOMNode } = require("devtools/client/shared/vendor/react-dom");
const Actions = require("../actions/index");
const { getLongString } = require("../utils/client");
const { Prefs } = require("../utils/prefs");
const { getFormDataSections } = require("../utils/request-utils");
const { getSelectedRequest } = require("../selectors/index");

// Components
const SplitBox = createFactory(require("devtools/client/shared/components/splitter/split-box"));
const NetworkDetailsPanel = createFactory(require("../shared/components/network-details-panel"));
const RequestList = createFactory(require("./request-list"));
const Toolbar = createFactory(require("./toolbar"));

const { div } = DOM;
const MediaQueryList = window.matchMedia("(min-width: 700px)");

/*
 * Monitor panel component
 * The main panel for displaying various network request information
 */
const MonitorPanel = createClass({
  displayName: "MonitorPanel",

  propTypes: {
    isEmpty: PropTypes.bool.isRequired,
    networkDetailsOpen: PropTypes.bool.isRequired,
    openNetworkDetails: PropTypes.func.isRequired,
    request: PropTypes.object,
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
        getLongString,
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
      Prefs.networkDetailsWidth = clientWidth;
    }
    if (!this.state.isVerticalSpliter && clientHeight) {
      Prefs.networkDetailsHeight = clientHeight;
    }
  },

  onLayoutChange() {
    this.setState({
      isVerticalSpliter: MediaQueryList.matches,
    });
  },

  render() {
    let { isEmpty, networkDetailsOpen } = this.props;
    return (
      div({ className: "monitor-panel" },
        Toolbar(),
        SplitBox({
          className: "devtools-responsive-container",
          initialWidth: `${Prefs.networkDetailsWidth}px`,
          initialHeight: `${Prefs.networkDetailsHeight}px`,
          minSize: "50px",
          maxSize: "80%",
          splitterSize: "1px",
          startPanel: RequestList({ isEmpty }),
          endPanel: networkDetailsOpen && NetworkDetailsPanel({ ref: "endPanel" }),
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
