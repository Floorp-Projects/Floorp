/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/redux/visibility-handler-connect");

const Actions = require("../actions/index");
const { FILTER_SEARCH_DELAY, FILTER_TAGS } = require("../constants");
const {
  getDisplayedRequests,
  getRecordingState,
  getTypeFilteredRequests,
} = require("../selectors/index");
const { autocompleteProvider } = require("../utils/filter-autocomplete-provider");
const { L10N } = require("../utils/l10n");
const { fetchNetworkUpdatePacket } = require("../utils/request-utils");

// MDN
const {
  getFilterBoxURL,
} = require("../utils/mdn-utils");
const LEARN_MORE_URL = getFilterBoxURL();

// Components
const SearchBox = createFactory(require("devtools/client/shared/components/SearchBox"));

const { button, div, input, label, span } = dom;

// Localization
const SEARCH_KEY_SHORTCUT = L10N.getStr("netmonitor.toolbar.filterFreetext.key");
const SEARCH_PLACE_HOLDER = L10N.getStr("netmonitor.toolbar.filterFreetext.label");
const TOOLBAR_CLEAR = L10N.getStr("netmonitor.toolbar.clear");
const TOOLBAR_TOGGLE_RECORDING = L10N.getStr("netmonitor.toolbar.toggleRecording");
const TOOLBAR_HAR_BUTTON = L10N.getStr("netmonitor.label.har");
const LEARN_MORE_TITLE = L10N.getStr("netmonitor.toolbar.filterFreetext.learnMore");

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

// Menu
loader.lazyRequireGetter(this, "showMenu", "devtools/client/netmonitor/src/utils/menu", true);
loader.lazyRequireGetter(this, "HarMenuUtils", "devtools/client/netmonitor/src/har/har-menu-utils", true);

// Throttling
const Types = require("devtools/client/shared/components/throttling/types");
const NetworkThrottlingSelector = createFactory(require("devtools/client/shared/components/throttling/NetworkThrottlingSelector"));
const { changeNetworkThrottling } = require("devtools/client/shared/components/throttling/actions");

/**
 * Network monitor toolbar component.
 *
 * Toolbar contains a set of useful tools to control network requests
 * as well as set of filters for filtering the content.
 */
class Toolbar extends Component {
  static get propTypes() {
    return {
      actions: PropTypes.object.isRequired,
      connector: PropTypes.object.isRequired,
      toggleRecording: PropTypes.func.isRequired,
      recording: PropTypes.bool.isRequired,
      clearRequests: PropTypes.func.isRequired,
      // List of currently displayed requests (i.e. filtered & sorted).
      displayedRequests: PropTypes.array.isRequired,
      requestFilterTypes: PropTypes.object.isRequired,
      setRequestFilterText: PropTypes.func.isRequired,
      enablePersistentLogs: PropTypes.func.isRequired,
      togglePersistentLogs: PropTypes.func.isRequired,
      persistentLogsEnabled: PropTypes.bool.isRequired,
      disableBrowserCache: PropTypes.func.isRequired,
      toggleBrowserCache: PropTypes.func.isRequired,
      browserCacheDisabled: PropTypes.bool.isRequired,
      toggleRequestFilterType: PropTypes.func.isRequired,
      filteredRequests: PropTypes.array.isRequired,
      // Set to true if there is enough horizontal space
      // and the toolbar needs just one row.
      singleRow: PropTypes.bool.isRequired,
      // Callback for opening split console.
      openSplitConsole: PropTypes.func,
      networkThrottling: PropTypes.shape(Types.networkThrottling).isRequired,
      // Executed when throttling changes (through toolbar button).
      onChangeNetworkThrottling: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.autocompleteProvider = this.autocompleteProvider.bind(this);
    this.onSearchBoxFocus = this.onSearchBoxFocus.bind(this);
    this.toggleRequestFilterType = this.toggleRequestFilterType.bind(this);
    this.updatePersistentLogsEnabled = this.updatePersistentLogsEnabled.bind(this);
    this.updateBrowserCacheDisabled = this.updateBrowserCacheDisabled.bind(this);
  }

  componentDidMount() {
    Services.prefs.addObserver(DEVTOOLS_ENABLE_PERSISTENT_LOG_PREF,
                               this.updatePersistentLogsEnabled);
    Services.prefs.addObserver(DEVTOOLS_DISABLE_CACHE_PREF,
                               this.updateBrowserCacheDisabled);
  }

  shouldComponentUpdate(nextProps) {
    return this.props.persistentLogsEnabled !== nextProps.persistentLogsEnabled
    || this.props.browserCacheDisabled !== nextProps.browserCacheDisabled
    || this.props.recording !== nextProps.recording
    || this.props.singleRow !== nextProps.singleRow
    || !Object.is(this.props.requestFilterTypes, nextProps.requestFilterTypes)
    || this.props.networkThrottling !== nextProps.networkThrottling

    // Filtered requests are useful only when searchbox is focused
    || !!(this.refs.searchbox && this.refs.searchbox.focused);
  }

  componentWillUnmount() {
    Services.prefs.removeObserver(DEVTOOLS_ENABLE_PERSISTENT_LOG_PREF,
                                  this.updatePersistentLogsEnabled);
    Services.prefs.removeObserver(DEVTOOLS_DISABLE_CACHE_PREF,
                                  this.updateBrowserCacheDisabled);
  }

  toggleRequestFilterType(evt) {
    if (evt.type === "keydown" && (evt.key !== "" || evt.key !== "Enter")) {
      return;
    }
    this.props.toggleRequestFilterType(evt.target.dataset.key);
  }

  updatePersistentLogsEnabled() {
    this.props.enablePersistentLogs(
      Services.prefs.getBoolPref(DEVTOOLS_ENABLE_PERSISTENT_LOG_PREF));
  }

  updateBrowserCacheDisabled() {
    this.props.disableBrowserCache(
      Services.prefs.getBoolPref(DEVTOOLS_DISABLE_CACHE_PREF));
  }

  autocompleteProvider(filter) {
    return autocompleteProvider(filter, this.props.filteredRequests);
  }

  onSearchBoxFocus() {
    const { connector, filteredRequests } = this.props;

    // Fetch responseCookies & responseHeaders for building autocomplete list
    filteredRequests.forEach((request) => {
      fetchNetworkUpdatePacket(connector.requestData, request, [
        "responseCookies",
        "responseHeaders",
      ]);
    });
  }

  /**
   * Render a separator.
   */
  renderSeparator() {
    return span({ className: "devtools-separator" });
  }

  /**
   * Render a clear button.
   */
  renderClearButton(clearRequests) {
    return (
      button({
        className: "devtools-button devtools-clear-icon requests-list-clear-button",
        title: TOOLBAR_CLEAR,
        onClick: clearRequests,
      })
    );
  }

  /**
   * Render a ToggleRecording button.
   */
  renderToggleRecordingButton(recording, toggleRecording) {
    // Calculate class-list for toggle recording button.
    // The button has two states: pause/play.
    const toggleRecordingButtonClass = [
      "devtools-button",
      "requests-list-pause-button",
      recording ? "devtools-pause-icon" : "devtools-play-icon",
    ].join(" ");

    return (
      button({
        className: toggleRecordingButtonClass,
        title: TOOLBAR_TOGGLE_RECORDING,
        onClick: toggleRecording,
      })
    );
  }

  /**
   * Render filter buttons.
   */
  renderFilterButtons(requestFilterTypes) {
    // Render list of filter-buttons.
    const buttons = Object.entries(requestFilterTypes).map(([type, checked]) => {
      const classList = ["devtools-button", `requests-list-filter-${type}-button`];
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

    return div({ className: "requests-list-filter-buttons" }, buttons);
  }

  /**
   * Render a Persistlog checkbox.
   */
  renderPersistlogCheckbox(persistentLogsEnabled, togglePersistentLogs) {
    return (
      label(
        {
          className: "devtools-checkbox-label devtools-persistlog-checkbox",
          title: ENABLE_PERSISTENT_LOGS_TOOLTIP,
        },
        input({
          id: "devtools-persistlog-checkbox",
          className: "devtools-checkbox",
          type: "checkbox",
          checked: persistentLogsEnabled,
          onChange: togglePersistentLogs,
        }),
        ENABLE_PERSISTENT_LOGS_LABEL,
      )
    );
  }

  /**
   * Render a Cache checkbox.
   */
  renderCacheCheckbox(browserCacheDisabled, toggleBrowserCache) {
    return (
      label(
        {
          className: "devtools-checkbox-label devtools-cache-checkbox",
          title: DISABLE_CACHE_TOOLTIP,
        },
        input({
          id: "devtools-cache-checkbox",
          className: "devtools-checkbox",
          type: "checkbox",
          checked: browserCacheDisabled,
          onChange: toggleBrowserCache,
        }),
        DISABLE_CACHE_LABEL,
      )
    );
  }

  /**
   * Render network throttling selector button.
   */
  renderThrottlingSelector() {
    const {
      networkThrottling,
      onChangeNetworkThrottling,
    } = this.props;

    return NetworkThrottlingSelector({
      className: "devtools-button",
      networkThrottling,
      onChangeNetworkThrottling,
    });
  }

  /**
   * Render drop down button with HAR related actions.
   */
  renderHarButton() {
    return button({
      id: "devtools-har-button",
      title: TOOLBAR_HAR_BUTTON,
      className: "devtools-button devtools-har-button",
      onClick: evt => {
        this.showHarMenu(evt.target);
      },
    });
  }

  showHarMenu(menuButton) {
    const {
      actions,
      connector,
      displayedRequests,
      openSplitConsole,
    } = this.props;

    const menuItems = [];

    menuItems.push({
      id: "request-list-context-import-har",
      label: L10N.getStr("netmonitor.context.importHar"),
      accesskey: L10N.getStr("netmonitor.context.importHar.accesskey"),
      click: () => HarMenuUtils.openHarFile(actions, openSplitConsole),
    });

    menuItems.push("-");

    menuItems.push({
      id: "request-list-context-save-all-as-har",
      label: L10N.getStr("netmonitor.context.saveAllAsHar"),
      accesskey: L10N.getStr("netmonitor.context.saveAllAsHar.accesskey"),
      disabled: !displayedRequests.length,
      click: () => HarMenuUtils.saveAllAsHar(displayedRequests, connector),
    });

    menuItems.push({
      id: "request-list-context-copy-all-as-har",
      label: L10N.getStr("netmonitor.context.copyAllAsHar"),
      accesskey: L10N.getStr("netmonitor.context.copyAllAsHar.accesskey"),
      disabled: !displayedRequests.length,
      click: () => HarMenuUtils.copyAllAsHar(displayedRequests, connector),
    });

    showMenu(menuItems, { button: menuButton });
  }

  /**
   * Render filter Searchbox.
   */
  renderFilterBox(setRequestFilterText) {
    return (
      SearchBox({
        delay: FILTER_SEARCH_DELAY,
        keyShortcut: SEARCH_KEY_SHORTCUT,
        placeholder: SEARCH_PLACE_HOLDER,
        plainStyle: true,
        type: "filter",
        ref: "searchbox",
        onChange: setRequestFilterText,
        onFocus: this.onSearchBoxFocus,
        autocompleteProvider: this.autocompleteProvider,
        learnMoreUrl: LEARN_MORE_URL,
        learnMoreTitle: LEARN_MORE_TITLE,
      })
    );
  }

  render() {
    const {
      toggleRecording,
      clearRequests,
      requestFilterTypes,
      setRequestFilterText,
      togglePersistentLogs,
      persistentLogsEnabled,
      toggleBrowserCache,
      browserCacheDisabled,
      recording,
      singleRow,
    } = this.props;

    // Render the entire toolbar.
    // dock at bottom or dock at side has different layout
    return singleRow ? (
      span({ className: "devtools-toolbar devtools-toolbar-container" },
        span({ className: "devtools-toolbar-group devtools-toolbar-single-row" },
          this.renderClearButton(clearRequests),
          this.renderSeparator(),
          this.renderFilterBox(setRequestFilterText),
          this.renderSeparator(),
          this.renderToggleRecordingButton(recording, toggleRecording),
          this.renderSeparator(),
          this.renderFilterButtons(requestFilterTypes),
          this.renderSeparator(),
          this.renderPersistlogCheckbox(persistentLogsEnabled, togglePersistentLogs),
          this.renderCacheCheckbox(browserCacheDisabled, toggleBrowserCache),
          this.renderSeparator(),
          this.renderThrottlingSelector(),
          this.renderHarButton(),
        )
      )
    ) : (
      span({ className: "devtools-toolbar devtools-toolbar-container" },
        span({ className: "devtools-toolbar-group devtools-toolbar-two-rows-1" },
          this.renderClearButton(clearRequests),
          this.renderSeparator(),
          this.renderFilterBox(setRequestFilterText),
          this.renderSeparator(),
          this.renderToggleRecordingButton(recording, toggleRecording),
          this.renderSeparator(),
          this.renderPersistlogCheckbox(persistentLogsEnabled, togglePersistentLogs),
          this.renderCacheCheckbox(browserCacheDisabled, toggleBrowserCache),
          this.renderSeparator(),
          this.renderThrottlingSelector(),
          this.renderHarButton(),
        ),
        span({ className: "devtools-toolbar-group devtools-toolbar-two-rows-2" },
          this.renderFilterButtons(requestFilterTypes)
        )
      )
    );
  }
}

module.exports = connect(
  (state) => ({
    browserCacheDisabled: state.ui.browserCacheDisabled,
    displayedRequests: getDisplayedRequests(state),
    filteredRequests: getTypeFilteredRequests(state),
    persistentLogsEnabled: state.ui.persistentLogsEnabled,
    recording: getRecordingState(state),
    requestFilterTypes: state.filters.requestFilterTypes,
    networkThrottling: state.networkThrottling,
  }),
  (dispatch) => ({
    clearRequests: () => dispatch(Actions.clearRequests()),
    disableBrowserCache: (disabled) => dispatch(Actions.disableBrowserCache(disabled)),
    enablePersistentLogs: (enabled) => dispatch(Actions.enablePersistentLogs(enabled)),
    setRequestFilterText: (text) => dispatch(Actions.setRequestFilterText(text)),
    toggleBrowserCache: () => dispatch(Actions.toggleBrowserCache()),
    toggleRecording: () => dispatch(Actions.toggleRecording()),
    togglePersistentLogs: () => dispatch(Actions.togglePersistentLogs()),
    toggleRequestFilterType: (type) => dispatch(Actions.toggleRequestFilterType(type)),
    onChangeNetworkThrottling: (enabled, profile) =>
      dispatch(changeNetworkThrottling(enabled, profile)),
  }),
)(Toolbar);
