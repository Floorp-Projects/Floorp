/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const {
  connect,
} = require("devtools/client/shared/redux/visibility-handler-connect");
const Actions = require("devtools/client/netmonitor/src/actions/index");
const {
  FILTER_SEARCH_DELAY,
  FILTER_TAGS,
  PANELS,
} = require("devtools/client/netmonitor/src/constants");
const {
  getDisplayedRequests,
  getRecordingState,
  getTypeFilteredRequests,
  getSelectedRequest,
} = require("devtools/client/netmonitor/src/selectors/index");
const {
  autocompleteProvider,
} = require("devtools/client/netmonitor/src/utils/filter-autocomplete-provider");
const { L10N } = require("devtools/client/netmonitor/src/utils/l10n");
const {
  fetchNetworkUpdatePacket,
} = require("devtools/client/netmonitor/src/utils/request-utils");

loader.lazyRequireGetter(
  this,
  "KeyShortcuts",
  "devtools/client/shared/key-shortcuts"
);

// MDN
const {
  getFilterBoxURL,
} = require("devtools/client/netmonitor/src/utils/mdn-utils");
const LEARN_MORE_URL = getFilterBoxURL();

// Components
const NetworkThrottlingMenu = createFactory(
  require("devtools/client/shared/components/throttling/NetworkThrottlingMenu")
);
const SearchBox = createFactory(
  require("devtools/client/shared/components/SearchBox")
);

const { button, div, input, label, span, hr } = dom;

// Localization
const FILTER_KEY_SHORTCUT = L10N.getStr(
  "netmonitor.toolbar.filterFreetext.key"
);
const SEARCH_KEY_SHORTCUT = L10N.getStr("netmonitor.toolbar.search.key");
const SEARCH_PLACE_HOLDER = L10N.getStr(
  "netmonitor.toolbar.filterFreetext.label"
);
const COPY_KEY_SHORTCUT = L10N.getStr("netmonitor.toolbar.copy.key");
const TOOLBAR_CLEAR = L10N.getStr("netmonitor.toolbar.clear");
const TOOLBAR_TOGGLE_RECORDING = L10N.getStr(
  "netmonitor.toolbar.toggleRecording"
);
const TOOLBAR_SEARCH = L10N.getStr("netmonitor.toolbar.search");
const TOOLBAR_BLOCKING = L10N.getStr("netmonitor.toolbar.requestBlocking");
const LEARN_MORE_TITLE = L10N.getStr(
  "netmonitor.toolbar.filterFreetext.learnMore"
);

// Preferences
const DEVTOOLS_DISABLE_CACHE_PREF = "devtools.cache.disabled";
const DEVTOOLS_ENABLE_PERSISTENT_LOG_PREF = "devtools.netmonitor.persistlog";
const TOOLBAR_FILTER_LABELS = FILTER_TAGS.concat("all").reduce(
  (o, tag) =>
    Object.assign(o, {
      [tag]: L10N.getStr(`netmonitor.toolbar.filter.${tag}`),
    }),
  {}
);
const DISABLE_CACHE_TOOLTIP = L10N.getStr(
  "netmonitor.toolbar.disableCache.tooltip"
);
const DISABLE_CACHE_LABEL = L10N.getStr(
  "netmonitor.toolbar.disableCache.label"
);

const MenuButton = createFactory(
  require("devtools/client/shared/components/menu/MenuButton")
);

loader.lazyGetter(this, "MenuItem", function() {
  return createFactory(
    require("devtools/client/shared/components/menu/MenuItem")
  );
});

loader.lazyGetter(this, "MenuList", function() {
  return createFactory(
    require("devtools/client/shared/components/menu/MenuList")
  );
});

// Menu
loader.lazyRequireGetter(
  this,
  "HarMenuUtils",
  "devtools/client/netmonitor/src/har/har-menu-utils",
  true
);
loader.lazyRequireGetter(
  this,
  "copyString",
  "devtools/shared/platform/clipboard",
  true
);

// Throttling
const Types = require("devtools/client/shared/components/throttling/types");
const {
  changeNetworkThrottling,
} = require("devtools/client/shared/components/throttling/actions");

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
      toggleSearchPanel: PropTypes.func.isRequired,
      networkActionBarOpen: PropTypes.bool,
      toggleRequestBlockingPanel: PropTypes.func.isRequired,
      networkActionBarSelectedPanel: PropTypes.string.isRequired,
      hasBlockedRequests: PropTypes.bool.isRequired,
      selectedRequest: PropTypes.object,
      toolboxDoc: PropTypes.object.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.autocompleteProvider = this.autocompleteProvider.bind(this);
    this.onSearchBoxFocus = this.onSearchBoxFocus.bind(this);
    this.toggleRequestFilterType = this.toggleRequestFilterType.bind(this);
    this.updatePersistentLogsEnabled = this.updatePersistentLogsEnabled.bind(
      this
    );
    this.updateBrowserCacheDisabled = this.updateBrowserCacheDisabled.bind(
      this
    );
  }

  componentDidMount() {
    Services.prefs.addObserver(
      DEVTOOLS_ENABLE_PERSISTENT_LOG_PREF,
      this.updatePersistentLogsEnabled
    );
    Services.prefs.addObserver(
      DEVTOOLS_DISABLE_CACHE_PREF,
      this.updateBrowserCacheDisabled
    );

    this.shortcuts = new KeyShortcuts({
      window,
    });

    this.shortcuts.on(SEARCH_KEY_SHORTCUT, event => {
      event.preventDefault();
      this.props.toggleSearchPanel();
    });

    this.shortcuts.on(COPY_KEY_SHORTCUT, () => {
      if (this.props.selectedRequest && this.props.selectedRequest.url) {
        copyString(this.props.selectedRequest.url);
      }
    });
  }

  shouldComponentUpdate(nextProps) {
    return (
      this.props.persistentLogsEnabled !== nextProps.persistentLogsEnabled ||
      this.props.browserCacheDisabled !== nextProps.browserCacheDisabled ||
      this.props.recording !== nextProps.recording ||
      this.props.networkActionBarOpen !== nextProps.networkActionBarOpen ||
      this.props.singleRow !== nextProps.singleRow ||
      !Object.is(this.props.requestFilterTypes, nextProps.requestFilterTypes) ||
      this.props.networkThrottling !== nextProps.networkThrottling ||
      // Filtered requests are useful only when searchbox is focused
      !!(this.refs.searchbox && this.refs.searchbox.focused) ||
      this.props.networkActionBarSelectedPanel !==
        nextProps.networkActionBarSelectedPanel ||
      this.props.hasBlockedRequests !== nextProps.hasBlockedRequests
    );
  }

  componentWillUnmount() {
    Services.prefs.removeObserver(
      DEVTOOLS_ENABLE_PERSISTENT_LOG_PREF,
      this.updatePersistentLogsEnabled
    );
    Services.prefs.removeObserver(
      DEVTOOLS_DISABLE_CACHE_PREF,
      this.updateBrowserCacheDisabled
    );

    if (this.shortcuts) {
      this.shortcuts.destroy();
    }
  }

  toggleRequestFilterType(evt) {
    if (evt.type === "keydown" && (evt.key !== "" || evt.key !== "Enter")) {
      return;
    }
    this.props.toggleRequestFilterType(evt.target.dataset.key);
  }

  updatePersistentLogsEnabled() {
    // Make sure the UI is updated when the pref changes.
    // It might happen when the user changed it through about:config or
    // through another Toolbox instance (opened in another browser tab).
    // In such case, skip telemetry recordings.
    this.props.enablePersistentLogs(
      Services.prefs.getBoolPref(DEVTOOLS_ENABLE_PERSISTENT_LOG_PREF),
      true
    );
  }

  updateBrowserCacheDisabled() {
    this.props.disableBrowserCache(
      Services.prefs.getBoolPref(DEVTOOLS_DISABLE_CACHE_PREF)
    );
  }

  autocompleteProvider(filter) {
    return autocompleteProvider(filter, this.props.filteredRequests);
  }

  onSearchBoxFocus() {
    const { connector, filteredRequests } = this.props;

    // Fetch responseCookies & responseHeaders for building autocomplete list
    filteredRequests.forEach(request => {
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
    return button({
      className:
        "devtools-button devtools-clear-icon requests-list-clear-button",
      title: TOOLBAR_CLEAR,
      onClick: clearRequests,
    });
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

    return button({
      className: toggleRecordingButtonClass,
      title: TOOLBAR_TOGGLE_RECORDING,
      onClick: toggleRecording,
    });
  }

  /**
   * Render a blocking button.
   */
  renderBlockingButton(toggleSearchPanel) {
    const {
      networkActionBarOpen,
      toggleRequestBlockingPanel,
      networkActionBarSelectedPanel,
      hasBlockedRequests,
    } = this.props;

    // The blocking feature is available behind a pref.
    if (
      !Services.prefs.getBoolPref(
        "devtools.netmonitor.features.requestBlocking"
      )
    ) {
      return null;
    }

    const className = ["devtools-button", "requests-list-blocking-button"];
    if (
      networkActionBarOpen &&
      networkActionBarSelectedPanel === PANELS.BLOCKING
    ) {
      className.push("checked");
    }

    if (hasBlockedRequests) {
      className.push("requests-list-blocking-button-enabled");
    }

    return button({
      className: className.join(" "),
      title: TOOLBAR_BLOCKING,
      "aria-pressed": networkActionBarOpen,
      onClick: toggleRequestBlockingPanel,
    });
  }

  /**
   * Render a search button.
   */
  renderSearchButton(toggleSearchPanel) {
    const { networkActionBarOpen, networkActionBarSelectedPanel } = this.props;

    // The search feature is available behind a pref.
    if (!Services.prefs.getBoolPref("devtools.netmonitor.features.search")) {
      return null;
    }

    const className = [
      "devtools-button",
      "devtools-search-icon",
      "requests-list-search-button",
    ];

    if (
      networkActionBarOpen &&
      networkActionBarSelectedPanel === PANELS.SEARCH
    ) {
      className.push("checked");
    }

    return button({
      className: className.join(" "),
      title: TOOLBAR_SEARCH,
      "aria-pressed": networkActionBarOpen,
      onClick: toggleSearchPanel,
    });
  }

  /**
   * Render filter buttons.
   */
  renderFilterButtons(requestFilterTypes) {
    // Render list of filter-buttons.
    const buttons = Object.entries(requestFilterTypes).map(([type, checked]) =>
      button(
        {
          className: `devtools-togglebutton requests-list-filter-${type}-button`,
          key: type,
          onClick: this.toggleRequestFilterType,
          onKeyDown: this.toggleRequestFilterType,
          "aria-pressed": checked,
          "data-key": type,
        },
        TOOLBAR_FILTER_LABELS[type]
      )
    );
    return div({ className: "requests-list-filter-buttons" }, buttons);
  }

  /**
   * Render a Cache checkbox.
   */
  renderCacheCheckbox(browserCacheDisabled, toggleBrowserCache) {
    return label(
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
      DISABLE_CACHE_LABEL
    );
  }

  /**
   * Render network throttling menu button.
   */
  renderThrottlingMenu() {
    const { networkThrottling, onChangeNetworkThrottling } = this.props;

    return NetworkThrottlingMenu({
      networkThrottling,
      onChangeNetworkThrottling,
    });
  }

  /**
   * Render filter Searchbox.
   */
  renderFilterBox(setRequestFilterText) {
    return SearchBox({
      delay: FILTER_SEARCH_DELAY,
      keyShortcut: FILTER_KEY_SHORTCUT,
      placeholder: SEARCH_PLACE_HOLDER,
      type: "filter",
      ref: "searchbox",
      onChange: setRequestFilterText,
      onFocus: this.onSearchBoxFocus,
      autocompleteProvider: this.autocompleteProvider,
      learnMoreUrl: LEARN_MORE_URL,
      learnMoreTitle: LEARN_MORE_TITLE,
    });
  }

  renderSettingsMenuButton() {
    const { toolboxDoc } = this.props;
    return MenuButton(
      {
        menuId: "netmonitor-settings-menu-button",
        toolboxDoc,
        className: "devtools-button netmonitor-settings-menu-button",
        title: L10N.getStr("netmonitor.settings.menuTooltip"),
      },
      // We pass the children in a function so we don't require the MenuItem and MenuList
      // components until we need to display them (i.e. when the button is clicked).
      () => this.renderSettingsMenuItems()
    );
  }

  renderSettingsMenuItems() {
    const {
      actions,
      connector,
      displayedRequests,
      openSplitConsole,
      persistentLogsEnabled,
      togglePersistentLogs,
    } = this.props;

    const menuItems = [
      MenuItem({
        key: "netmonitor-settings-persist-item",
        className: "menu-item netmonitor-settings-persist-item",
        type: "checkbox",
        checked: persistentLogsEnabled,
        label: L10N.getStr("netmonitor.toolbar.enablePersistentLogs.label"),
        tooltip: L10N.getStr("netmonitor.toolbar.enablePersistentLogs.tooltip"),
        onClick: () => togglePersistentLogs(),
      }),
      hr({ key: "netmonitor-settings-har-divider" }),
      MenuItem({
        key: "request-list-context-import-har",
        className: "menu-item netmonitor-settings-import-har-item",
        label: L10N.getStr("netmonitor.har.importHarDialogTitle"),
        tooltip: L10N.getStr("netmonitor.settings.importHarTooltip"),
        accesskey: L10N.getStr("netmonitor.context.importHar.accesskey"),
        onClick: () => HarMenuUtils.openHarFile(actions, openSplitConsole),
      }),
      MenuItem({
        key: "request-list-context-save-all-as-har",
        className: "menu-item netmonitor-settings-save-har-item",
        label: L10N.getStr("netmonitor.context.saveAllAsHar"),
        accesskey: L10N.getStr("netmonitor.context.saveAllAsHar.accesskey"),
        tooltip: L10N.getStr("netmonitor.settings.saveHarTooltip"),
        disabled: !displayedRequests.length,
        onClick: () => HarMenuUtils.saveAllAsHar(displayedRequests, connector),
      }),
      MenuItem({
        key: "request-list-context-copy-all-as-har",
        className: "menu-item netmonitor-settings-copy-har-item",
        label: L10N.getStr("netmonitor.context.copyAllAsHar"),
        accesskey: L10N.getStr("netmonitor.context.copyAllAsHar.accesskey"),
        tooltip: L10N.getStr("netmonitor.settings.copyHarTooltip"),
        disabled: !displayedRequests.length,
        onClick: () => HarMenuUtils.copyAllAsHar(displayedRequests, connector),
      }),
    ];

    return MenuList({ id: "netmonitor-settings-menu-list" }, menuItems);
  }

  render() {
    const {
      toggleRecording,
      clearRequests,
      requestFilterTypes,
      setRequestFilterText,
      toggleBrowserCache,
      browserCacheDisabled,
      recording,
      singleRow,
      toggleSearchPanel,
    } = this.props;

    // Render the entire toolbar.
    // dock at bottom or dock at side has different layout
    return singleRow
      ? span(
          { id: "netmonitor-toolbar-container" },
          span(
            { className: "devtools-toolbar devtools-input-toolbar" },
            this.renderClearButton(clearRequests),
            this.renderSeparator(),
            this.renderFilterBox(setRequestFilterText),
            this.renderSeparator(),
            this.renderToggleRecordingButton(recording, toggleRecording),
            this.renderSearchButton(toggleSearchPanel),
            this.renderBlockingButton(toggleSearchPanel),
            this.renderSeparator(),
            this.renderFilterButtons(requestFilterTypes),
            this.renderSeparator(),
            this.renderCacheCheckbox(browserCacheDisabled, toggleBrowserCache),
            this.renderSeparator(),
            this.renderThrottlingMenu(),
            this.renderSeparator(),
            this.renderSettingsMenuButton()
          )
        )
      : span(
          { id: "netmonitor-toolbar-container" },
          span(
            { className: "devtools-toolbar devtools-input-toolbar" },
            this.renderClearButton(clearRequests),
            this.renderSeparator(),
            this.renderFilterBox(setRequestFilterText),
            this.renderSeparator(),
            this.renderToggleRecordingButton(recording, toggleRecording),
            this.renderSearchButton(toggleSearchPanel),
            this.renderBlockingButton(toggleSearchPanel),
            this.renderSeparator(),
            this.renderCacheCheckbox(browserCacheDisabled, toggleBrowserCache),
            this.renderSeparator(),
            this.renderThrottlingMenu(),
            this.renderSeparator(),
            this.renderSettingsMenuButton()
          ),
          span(
            { className: "devtools-toolbar devtools-input-toolbar" },
            this.renderFilterButtons(requestFilterTypes)
          )
        );
  }
}

module.exports = connect(
  state => ({
    browserCacheDisabled: state.ui.browserCacheDisabled,
    displayedRequests: getDisplayedRequests(state),
    hasBlockedRequests:
      state.requestBlocking.blockingEnabled &&
      state.requestBlocking.blockedUrls.some(({ enabled }) => enabled),
    filteredRequests: getTypeFilteredRequests(state),
    persistentLogsEnabled: state.ui.persistentLogsEnabled,
    recording: getRecordingState(state),
    requestFilterTypes: state.filters.requestFilterTypes,
    networkThrottling: state.networkThrottling,
    networkActionBarOpen: state.ui.networkActionOpen,
    networkActionBarSelectedPanel: state.ui.selectedActionBarTabId || "",
    selectedRequest: getSelectedRequest(state),
  }),
  dispatch => ({
    clearRequests: () => dispatch(Actions.clearRequests()),
    disableBrowserCache: disabled =>
      dispatch(Actions.disableBrowserCache(disabled)),
    enablePersistentLogs: (enabled, skipTelemetry) =>
      dispatch(Actions.enablePersistentLogs(enabled, skipTelemetry)),
    setRequestFilterText: text => dispatch(Actions.setRequestFilterText(text)),
    toggleBrowserCache: () => dispatch(Actions.toggleBrowserCache()),
    toggleRecording: () => dispatch(Actions.toggleRecording()),
    togglePersistentLogs: () => dispatch(Actions.togglePersistentLogs()),
    toggleRequestFilterType: type =>
      dispatch(Actions.toggleRequestFilterType(type)),
    onChangeNetworkThrottling: (enabled, profile) =>
      dispatch(changeNetworkThrottling(enabled, profile)),
    toggleSearchPanel: () => dispatch(Actions.toggleSearchPanel()),
    toggleRequestBlockingPanel: () =>
      dispatch(Actions.toggleRequestBlockingPanel()),
  })
)(Toolbar);
