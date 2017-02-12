/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */
/* globals gNetwork */

"use strict";

const {
  createClass,
  createFactory,
  DOM,
  PropTypes,
} = require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const { setNamedTimeout } = require("devtools/client/shared/widgets/view-helpers");
const Actions = require("../actions/index");
const { Prefs } = require("../prefs");
const { getFormDataSections } = require("../request-utils");
const {
  getActiveFilters,
  getSelectedRequest,
} = require("../selectors/index");

// Components
const RequestListContent = createFactory(require("./request-list-content"));
const RequestListEmptyNotice = createFactory(require("./request-list-empty"));
const RequestListHeader = createFactory(require("./request-list-header"));

const { div } = DOM;

/**
 * Request panel component
 */
const RequestList = createClass({
  displayName: "RequestList",

  propTypes: {
    activeFilters: PropTypes.array,
    dispatch: PropTypes.func,
    isEmpty: PropTypes.bool.isRequired,
    request: PropTypes.object,
    networkDetailsOpen: PropTypes.bool,
  },

  componentDidMount() {
    const { dispatch } = this.props;

    Prefs.filters.forEach((type) => dispatch(Actions.toggleRequestFilterType(type)));
    this.splitter = document.querySelector("#network-inspector-view-splitter");
    this.splitter.addEventListener("mouseup", this.resize);
    window.addEventListener("resize", this.resize);
  },

  componentWillReceiveProps(nextProps) {
    const { dispatch, request = {}, networkDetailsOpen } = this.props;

    if (nextProps.request && nextProps.request !== request) {
      dispatch(Actions.openNetworkDetails(true));
    }

    if (nextProps.networkDetailsOpen !== networkDetailsOpen) {
      this.resize();
    }

    const {
      formDataSections,
      requestHeaders,
      requestHeadersFromUploadStream,
      requestPostData,
    } = nextProps.request || {};

    if (!formDataSections && requestHeaders &&
        requestHeadersFromUploadStream && requestPostData) {
      getFormDataSections(
        requestHeaders,
        requestHeadersFromUploadStream,
        requestPostData,
        gNetwork.getString.bind(gNetwork),
      ).then((newFormDataSections) => {
        dispatch(Actions.updateRequest(
          request.id,
          { formDataSections: newFormDataSections },
          true,
        ));
      });
    }
  },

  componentWillUnmount() {
    Prefs.filters = this.props.activeFilters;
    this.splitter.removeEventListener("mouseup", this.resize);
    window.removeEventListener("resize", this.resize);
  },

  resize() {
    const { dispatch } = this.props;
    // Allow requests to settle down first.
    setNamedTimeout("resize-events", 50, () => {
      const waterfallHeaderEl =
        document.querySelector("#requests-menu-waterfall-header-box");
      if (waterfallHeaderEl) {
        const { width } = waterfallHeaderEl.getBoundingClientRect();
        dispatch(Actions.resizeWaterfall(width));
      }
    });
  },

  render() {
    return (
      div({ className: "request-list-container" },
        RequestListHeader(),
        this.props.isEmpty ? RequestListEmptyNotice() : RequestListContent(),
      )
    );
  }
});

module.exports = connect(
  (state) => ({
    activeFilters: getActiveFilters(state),
    isEmpty: state.requests.requests.isEmpty(),
    request: getSelectedRequest(state),
    networkDetailsOpen: state.ui.networkDetailsOpen,
  })
)(RequestList);
