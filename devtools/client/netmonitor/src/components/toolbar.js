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
const Actions = require("../actions/index");
const { FILTER_SEARCH_DELAY, FILTER_FLAGS } = require("../constants");
const {
  getDisplayedRequestsSummary,
  getRequestFilterTypes,
  isNetworkDetailsToggleButtonDisabled,
} = require("../selectors/index");

const { L10N } = require("../utils/l10n");

// Components
const SearchBox = createFactory(require("devtools/client/shared/components/search-box"));

const { button, div, span } = DOM;

const COLLPASE_DETAILS_PANE = L10N.getStr("collapseDetailsPane");
const EXPAND_DETAILS_PANE = L10N.getStr("expandDetailsPane");
const SEARCH_KEY_SHORTCUT = L10N.getStr("netmonitor.toolbar.filterFreetext.key");
const SEARCH_PLACE_HOLDER = L10N.getStr("netmonitor.toolbar.filterFreetext.label");
const TOOLBAR_CLEAR = L10N.getStr("netmonitor.toolbar.clear");

/*
 * Network monitor toolbar component
 * Toolbar contains a set of useful tools to control network requests
 */
const Toolbar = createClass({
  displayName: "Toolbar",

  propTypes: {
    clearRequests: PropTypes.func.isRequired,
    requestFilterTypes: PropTypes.array.isRequired,
    setRequestFilterText: PropTypes.func.isRequired,
    networkDetailsToggleDisabled: PropTypes.bool.isRequired,
    networkDetailsOpen: PropTypes.bool.isRequired,
    toggleNetworkDetails: PropTypes.func.isRequired,
    toggleRequestFilterType: PropTypes.func.isRequired,
  },

  toggleRequestFilterType(evt) {
    if (evt.type === "keydown" && (evt.key !== "" || evt.key !== "Enter")) {
      return;
    }
    this.props.toggleRequestFilterType(evt.target.dataset.key);
  },

  render() {
    let {
      clearRequests,
      requestFilterTypes,
      setRequestFilterText,
      networkDetailsToggleDisabled,
      networkDetailsOpen,
      toggleNetworkDetails,
    } = this.props;

    let toggleButtonClassName = [
      "network-details-panel-toggle",
      "devtools-button",
    ];
    if (!networkDetailsOpen) {
      toggleButtonClassName.push("pane-collapsed");
    }

    let buttons = requestFilterTypes.map(([type, checked]) => {
      let classList = ["devtools-button", `requests-list-filter-${type}-button`];
      checked && classList.push("checked");

      return (
        button({
          className: classList.join(" "),
          key: type,
          onClick: this.toggleRequestFilterType,
          onKeyDown: this.toggleRequestFilterType,
          "aria-pressed": checked,
          "data-key": type,
        },
          L10N.getStr(`netmonitor.toolbar.filter.${type}`)
        )
      );
    });

    return (
      span({ className: "devtools-toolbar devtools-toolbar-container" },
        span({ className: "devtools-toolbar-group" },
          button({
            className: "devtools-button devtools-clear-icon requests-list-clear-button",
            title: TOOLBAR_CLEAR,
            onClick: clearRequests,
          }),
          div({ className: "requests-list-filter-buttons" }, buttons),
        ),
        span({ className: "devtools-toolbar-group" },
          SearchBox({
            delay: FILTER_SEARCH_DELAY,
            keyShortcut: SEARCH_KEY_SHORTCUT,
            placeholder: SEARCH_PLACE_HOLDER,
            type: "filter",
            onChange: setRequestFilterText,
            autocompleteList: FILTER_FLAGS.map((item) => `${item}:`),
          }),
          button({
            className: toggleButtonClassName.join(" "),
            title: networkDetailsOpen ? COLLPASE_DETAILS_PANE : EXPAND_DETAILS_PANE,
            disabled: networkDetailsToggleDisabled,
            tabIndex: "0",
            onClick: toggleNetworkDetails,
          }),
        )
      )
    );
  }
});

module.exports = connect(
  (state) => ({
    networkDetailsToggleDisabled: isNetworkDetailsToggleButtonDisabled(state),
    networkDetailsOpen: state.ui.networkDetailsOpen,
    requestFilterTypes: getRequestFilterTypes(state),
    summary: getDisplayedRequestsSummary(state),
  }),
  (dispatch) => ({
    clearRequests: () => dispatch(Actions.clearRequests()),
    setRequestFilterText: (text) => dispatch(Actions.setRequestFilterText(text)),
    toggleRequestFilterType: (type) => dispatch(Actions.toggleRequestFilterType(type)),
    toggleNetworkDetails: () => dispatch(Actions.toggleNetworkDetails()),
  }),
)(Toolbar);
