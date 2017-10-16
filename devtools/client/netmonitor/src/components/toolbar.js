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
const Actions = require("../actions/index");
const { FILTER_SEARCH_DELAY, FILTER_TAGS } = require("../constants");
const {
  getDisplayedRequestsSummary,
  getRecordingState,
  getRequestFilterTypes,
  getTypeFilteredRequests,
  isNetworkDetailsToggleButtonDisabled,
} = require("../selectors/index");

const { autocompleteProvider } = require("../utils/filter-autocomplete-provider");
const { L10N } = require("../utils/l10n");

// Components
const SearchBox = createFactory(require("devtools/client/shared/components/SearchBox"));

const { button, div, input, label, span } = DOM;

// Localization
const COLLAPSE_DETAILS_PANE = L10N.getStr("collapseDetailsPane");
const EXPAND_DETAILS_PANE = L10N.getStr("expandDetailsPane");
const SEARCH_KEY_SHORTCUT = L10N.getStr("netmonitor.toolbar.filterFreetext.key");
const SEARCH_PLACE_HOLDER = L10N.getStr("netmonitor.toolbar.filterFreetext.label");
const TOOLBAR_CLEAR = L10N.getStr("netmonitor.toolbar.clear");
const TOOLBAR_TOGGLE_RECORDING = L10N.getStr("netmonitor.toolbar.toggleRecording");

// Preferences
const DEVTOOLS_DISABLE_CACHE_PREF = "devtools.cache.disabled";
const DEVTOOLS_ENABLE_PERSISTENT_LOG_PREF = "devtools.netmonitor.persistlog";
const TOOLBAR_FILTER_LABELS = FILTER_TAGS.concat("all").reduce((o, tag) =>
  Object.assign(o, { [tag]: L10N.getStr(`netmonitor.toolbar.filter.${tag}`) }), {});
const ENABLE_PERSISTENT_LOGS_TOOLTIP =
  L10N.getStr("netmonitor.toolbar.enablePersistentLogs.tooltip");
const ENABLE_PERSISTENT_LOGS_LABEL =
  L10N.getStr("netmonitor.toolbar.enablePersistentLogs.label");
const DISABLE_CACHE_TOOLTIP = L10N.getStr("netmonitor.toolbar.disableCache.tooltip");
const DISABLE_CACHE_LABEL = L10N.getStr("netmonitor.toolbar.disableCache.label");

/**
 * Network monitor toolbar component.
 *
 * Toolbar contains a set of useful tools to control network requests
 * as well as set of filters for filtering the content.
 */
const Toolbar = createClass({
  displayName: "Toolbar",

  propTypes: {
    toggleRecording: PropTypes.func.isRequired,
    recording: PropTypes.bool.isRequired,
    clearRequests: PropTypes.func.isRequired,
    requestFilterTypes: PropTypes.array.isRequired,
    setRequestFilterText: PropTypes.func.isRequired,
    networkDetailsToggleDisabled: PropTypes.bool.isRequired,
    networkDetailsOpen: PropTypes.bool.isRequired,
    toggleNetworkDetails: PropTypes.func.isRequired,
    enablePersistentLogs: PropTypes.func.isRequired,
    togglePersistentLogs: PropTypes.func.isRequired,
    persistentLogsEnabled: PropTypes.bool.isRequired,
    disableBrowserCache: PropTypes.func.isRequired,
    toggleBrowserCache: PropTypes.func.isRequired,
    browserCacheDisabled: PropTypes.bool.isRequired,
    toggleRequestFilterType: PropTypes.func.isRequired,
    filteredRequests: PropTypes.object.isRequired,
  },

  componentDidMount() {
    Services.prefs.addObserver(DEVTOOLS_ENABLE_PERSISTENT_LOG_PREF,
                               this.updatePersistentLogsEnabled);
    Services.prefs.addObserver(DEVTOOLS_DISABLE_CACHE_PREF,
                               this.updateBrowserCacheDisabled);
  },

  componentWillUnmount() {
    Services.prefs.removeObserver(DEVTOOLS_ENABLE_PERSISTENT_LOG_PREF,
                                  this.updatePersistentLogsEnabled);
    Services.prefs.removeObserver(DEVTOOLS_DISABLE_CACHE_PREF,
                                  this.updateBrowserCacheDisabled);
  },

  toggleRequestFilterType(evt) {
    if (evt.type === "keydown" && (evt.key !== "" || evt.key !== "Enter")) {
      return;
    }
    this.props.toggleRequestFilterType(evt.target.dataset.key);
  },

  updatePersistentLogsEnabled() {
    this.props.enablePersistentLogs(
      Services.prefs.getBoolPref(DEVTOOLS_ENABLE_PERSISTENT_LOG_PREF));
  },

  updateBrowserCacheDisabled() {
    this.props.disableBrowserCache(
      Services.prefs.getBoolPref(DEVTOOLS_DISABLE_CACHE_PREF));
  },

  render() {
    let {
      toggleRecording,
      clearRequests,
      requestFilterTypes,
      setRequestFilterText,
      networkDetailsToggleDisabled,
      networkDetailsOpen,
      toggleNetworkDetails,
      togglePersistentLogs,
      persistentLogsEnabled,
      toggleBrowserCache,
      browserCacheDisabled,
      filteredRequests,
      recording,
    } = this.props;

    let toggleButtonClassName = [
      "network-details-panel-toggle",
      "devtools-button",
    ];

    if (!networkDetailsOpen) {
      toggleButtonClassName.push("pane-collapsed");
    }

    // Render list of filter-buttons.
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
          TOOLBAR_FILTER_LABELS[type]
        )
      );
    });

    // Calculate class-list for toggle recording button. The button
    // has two states: pause/play.
    let toggleButtonClassList = [
      "devtools-button",
      recording ? "devtools-pause-icon" : "devtools-play-icon",
    ];

    // Render the entire toolbar.
    return (
      span({ className: "devtools-toolbar devtools-toolbar-container" },
        span({ className: "devtools-toolbar-group" },
          button({
            className: toggleButtonClassList.join(" "),
            title: TOOLBAR_TOGGLE_RECORDING,
            onClick: toggleRecording,
          }),
          button({
            className: "devtools-button devtools-clear-icon requests-list-clear-button",
            title: TOOLBAR_CLEAR,
            onClick: clearRequests,
          }),
          div({ className: "requests-list-filter-buttons" }, buttons),
          label(
            {
              className: "devtools-checkbox-label",
              title: ENABLE_PERSISTENT_LOGS_TOOLTIP,
            },
            input({
              id: "devtools-persistlog-checkbox",
              className: "devtools-checkbox",
              type: "checkbox",
              checked: persistentLogsEnabled,
              onClick: togglePersistentLogs,
            }),
            ENABLE_PERSISTENT_LOGS_LABEL
          ),
          label(
            {
              className: "devtools-checkbox-label",
              title: DISABLE_CACHE_TOOLTIP,
            },
            input({
              id: "devtools-cache-checkbox",
              className: "devtools-checkbox",
              type: "checkbox",
              checked: browserCacheDisabled,
              onClick: toggleBrowserCache,
            }),
            DISABLE_CACHE_LABEL,
          ),
        ),
        span({ className: "devtools-toolbar-group" },
          SearchBox({
            delay: FILTER_SEARCH_DELAY,
            keyShortcut: SEARCH_KEY_SHORTCUT,
            placeholder: SEARCH_PLACE_HOLDER,
            type: "filter",
            onChange: setRequestFilterText,
            autocompleteProvider: filter =>
              autocompleteProvider(filter, filteredRequests),
          }),
          button({
            className: toggleButtonClassName.join(" "),
            title: networkDetailsOpen ? COLLAPSE_DETAILS_PANE : EXPAND_DETAILS_PANE,
            disabled: networkDetailsToggleDisabled,
            tabIndex: "0",
            onClick: toggleNetworkDetails,
          }),
        )
      )
    );
  },
});

module.exports = connect(
  (state) => ({
    networkDetailsToggleDisabled: isNetworkDetailsToggleButtonDisabled(state),
    networkDetailsOpen: state.ui.networkDetailsOpen,
    persistentLogsEnabled: state.ui.persistentLogsEnabled,
    browserCacheDisabled: state.ui.browserCacheDisabled,
    recording: getRecordingState(state),
    requestFilterTypes: getRequestFilterTypes(state),
    filteredRequests: getTypeFilteredRequests(state),
    summary: getDisplayedRequestsSummary(state),
  }),
  (dispatch) => ({
    clearRequests: () => dispatch(Actions.clearRequests()),
    disableBrowserCache: (disabled) => dispatch(Actions.disableBrowserCache(disabled)),
    enablePersistentLogs: (enabled) => dispatch(Actions.enablePersistentLogs(enabled)),
    setRequestFilterText: (text) => dispatch(Actions.setRequestFilterText(text)),
    toggleBrowserCache: () => dispatch(Actions.toggleBrowserCache()),
    toggleNetworkDetails: () => dispatch(Actions.toggleNetworkDetails()),
    toggleRecording: () => dispatch(Actions.toggleRecording()),
    togglePersistentLogs: () => dispatch(Actions.togglePersistentLogs()),
    toggleRequestFilterType: (type) => dispatch(Actions.toggleRequestFilterType(type)),
  }),
)(Toolbar);
