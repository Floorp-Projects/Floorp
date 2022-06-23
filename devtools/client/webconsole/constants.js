/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const actionTypes = {
  APPEND_NOTIFICATION: "APPEND_NOTIFICATION",
  APPEND_TO_HISTORY: "APPEND_TO_HISTORY",
  AUTOCOMPLETE_CLEAR: "AUTOCOMPLETE_CLEAR",
  AUTOCOMPLETE_DATA_RECEIVE: "AUTOCOMPLETE_DATA_RECEIVE",
  AUTOCOMPLETE_PENDING_REQUEST: "AUTOCOMPLETE_PENDING_REQUEST",
  AUTOCOMPLETE_RETRIEVE_FROM_CACHE: "AUTOCOMPLETE_RETRIEVE_FROM_CACHE",
  AUTOCOMPLETE_TOGGLE: "AUTOCOMPLETE_TOGGLE",
  BATCH_ACTIONS: "BATCH_ACTIONS",
  CLEAR_HISTORY: "CLEAR_HISTORY",
  ENABLE_NETWORK_MONITORING: "ENABLE_NETWORK_MONITORING",
  EDITOR_TOGGLE: "EDITOR_TOGGLE",
  EDITOR_ONBOARDING_DISMISS: "EDITOR_ONBOARDING_DISMISS",
  EDITOR_PRETTY_PRINT: "EDITOR_PRETTY_PRINT",
  EVALUATE_EXPRESSION: "EVALUATE_EXPRESSION",
  SET_TERMINAL_INPUT: "SET_TERMINAL_INPUT",
  SET_TERMINAL_EAGER_RESULT: "SET_TERMINAL_EAGER_RESULT",
  FILTER_TEXT_SET: "FILTER_TEXT_SET",
  FILTER_TOGGLE: "FILTER_TOGGLE",
  FILTERS_CLEAR: "FILTERS_CLEAR",
  FILTERBAR_DISPLAY_MODE_SET: "FILTERBAR_DISPLAY_MODE_SET",
  HISTORY_LOADED: "HISTORY_LOADED",
  INITIALIZE: "INITIALIZE",
  MESSAGE_CLOSE: "MESSAGE_CLOSE",
  MESSAGE_OPEN: "MESSAGE_OPEN",
  CSS_MESSAGE_ADD_MATCHING_ELEMENTS: "CSS_MESSAGE_ADD_MATCHING_ELEMENTS",
  MESSAGE_REMOVE: "MESSAGE_REMOVE",
  MESSAGES_ADD: "MESSAGES_ADD",
  MESSAGES_CLEAR: "MESSAGES_CLEAR",
  NETWORK_MESSAGES_UPDATE: "NETWORK_MESSAGES_UPDATE",
  NETWORK_UPDATES_REQUEST: "NETWORK_UPDATES_REQUEST",
  PERSIST_TOGGLE: "PERSIST_TOGGLE",
  PRIVATE_MESSAGES_CLEAR: "PRIVATE_MESSAGES_CLEAR",
  REMOVE_NOTIFICATION: "REMOVE_NOTIFICATION",
  FRONTS_TO_RELEASE_CLEAR: "FRONTS_TO_RELEASE_CLEAR",
  REVERSE_SEARCH_INPUT_TOGGLE: "REVERSE_SEARCH_INPUT_TOGGLE",
  SELECT_NETWORK_MESSAGE_TAB: "SELECT_NETWORK_MESSAGE_TAB",
  SHOW_OBJECT_IN_SIDEBAR: "SHOW_OBJECT_IN_SIDEBAR",
  SHOW_CONTENT_MESSAGES_TOGGLE: "SHOW_CONTENT_MESSAGES_TOGGLE",
  SIDEBAR_CLOSE: "SIDEBAR_CLOSE",
  SPLIT_CONSOLE_CLOSE_BUTTON_TOGGLE: "SPLIT_CONSOLE_CLOSE_BUTTON_TOGGLE",
  TIMESTAMPS_TOGGLE: "TIMESTAMPS_TOGGLE",
  UPDATE_HISTORY_POSITION: "UPDATE_HISTORY_POSITION",
  REVERSE_SEARCH_INPUT_CHANGE: "REVERSE_SEARCH_INPUT_CHANGE",
  REVERSE_SEARCH_NEXT: "REVERSE_SEARCH_NEXT",
  REVERSE_SEARCH_BACK: "REVERSE_SEARCH_BACK",
  EAGER_EVALUATION_TOGGLE: "EAGER_EVALUATION_TOGGLE",
  WARNING_GROUPS_TOGGLE: "WARNING_GROUPS_TOGGLE",
  WILL_NAVIGATE: "WILL_NAVIGATE",
  EDITOR_SET_WIDTH: "EDITOR_SET_WIDTH",
};

const prefs = {
  PREFS: {
    // Filter preferences only have the suffix since they can be used either for the
    // webconsole or the browser console.
    FILTER: {
      ERROR: "filter.error",
      WARN: "filter.warn",
      INFO: "filter.info",
      LOG: "filter.log",
      DEBUG: "filter.debug",
      CSS: "filter.css",
      NET: "filter.net",
      NETXHR: "filter.netxhr",
    },
    UI: {
      // Persist is only used by the webconsole.
      PERSIST: "devtools.webconsole.persistlog",
      // Max number of entries in history list.
      INPUT_HISTORY_COUNT: "devtools.webconsole.inputHistoryCount",
      // Is editor mode enabled.
      EDITOR: "input.editor",
      // Display content messages in the browser console.
      CONTENT_MESSAGES: "devtools.browserconsole.contentMessages",
      // Display timestamp in messages.
      MESSAGE_TIMESTAMP: "devtools.webconsole.timestampMessages",
      // Store the editor width.
      EDITOR_WIDTH: "input.editorWidth",
      // Show the Editor onboarding UI
      EDITOR_ONBOARDING: "devtools.webconsole.input.editorOnboarding",
      // Show the Input Context the selector
      CONTEXT_SELECTOR: "devtools.webconsole.input.context",
      // Persist the "enable network monitoring" option
      ENABLE_NETWORK_MONITORING:
        "devtools.browserconsole.enableNetworkMonitoring",
    },
    FEATURES: {
      // We use the same pref to enable the sidebar on webconsole and browser console.
      SIDEBAR_TOGGLE: "devtools.webconsole.sidebarToggle",
      AUTOCOMPLETE: "devtools.webconsole.input.autocomplete",
      EAGER_EVALUATION: "devtools.webconsole.input.eagerEvaluation",
      GROUP_WARNINGS: "devtools.webconsole.groupWarningMessages",
      BROWSER_TOOLBOX_FISSION: "devtools.browsertoolbox.fission",
    },
  },
};

const FILTERS = {
  CSS: "css",
  DEBUG: "debug",
  ERROR: "error",
  INFO: "info",
  LOG: "log",
  NET: "net",
  NETXHR: "netxhr",
  TEXT: "text",
  WARN: "warn",
};

const DEFAULT_FILTERS_VALUES = {
  [FILTERS.TEXT]: "",
  [FILTERS.ERROR]: true,
  [FILTERS.WARN]: true,
  [FILTERS.LOG]: true,
  [FILTERS.INFO]: true,
  [FILTERS.DEBUG]: true,
  [FILTERS.CSS]: false,
  [FILTERS.NET]: false,
  [FILTERS.NETXHR]: false,
};

const DEFAULT_FILTERS = Object.keys(DEFAULT_FILTERS_VALUES).filter(
  filter => DEFAULT_FILTERS_VALUES[filter] !== false
);

const chromeRDPEnums = {
  MESSAGE_SOURCE: {
    XML: "xml",
    CSS: "css",
    JAVASCRIPT: "javascript",
    NETWORK: "network",
    CONSOLE_API: "console-api",
    // Messages emitted by the console frontend itself (i.e. similar messages grouping
    // header).
    CONSOLE_FRONTEND: "console-frontend",
    STORAGE: "storage",
    APPCACHE: "appcache",
    RENDERING: "rendering",
    SECURITY: "security",
    OTHER: "other",
    DEPRECATION: "deprecation",
  },
  MESSAGE_TYPE: {
    LOG: "log",
    DIR: "dir",
    TABLE: "table",
    TRACE: "trace",
    CLEAR: "clear",
    START_GROUP: "startGroup",
    START_GROUP_COLLAPSED: "startGroupCollapsed",
    END_GROUP: "endGroup",
    CONTENT_BLOCKING_GROUP: "contentBlockingWarningGroup",
    STORAGE_ISOLATION_GROUP: "storageIsolationWarningGroup",
    TRACKING_PROTECTION_GROUP: "trackingProtectionWarningGroup",
    COOKIE_SAMESITE_GROUP: "cookieSameSiteGroup",
    CORS_GROUP: "CORSWarningGroup",
    CSP_GROUP: "CSPWarningGroup",
    ASSERT: "assert",
    DEBUG: "debug",
    PROFILE: "profile",
    PROFILE_END: "profileEnd",
    // Undocumented in Chrome RDP, but is used for evaluation results.
    RESULT: "result",
    // Undocumented in Chrome RDP, but is used for input.
    COMMAND: "command",
    // Undocumented in Chrome RDP, but is used for messages that should not
    // output anything (e.g. `console.time()` calls).
    NULL_MESSAGE: "nullMessage",
    NAVIGATION_MARKER: "navigationMarker",
    SIMPLE_TABLE: "simpleTable",
  },
  MESSAGE_LEVEL: {
    LOG: "log",
    ERROR: "error",
    WARN: "warn",
    DEBUG: "debug",
    INFO: "info",
  },
};

const jstermCommands = {
  JSTERM_COMMANDS: {
    INSPECT: "inspectObject",
  },
};

// Constants used for defining the direction of JSTerm input history navigation.
const historyCommands = {
  HISTORY_BACK: -1,
  HISTORY_FORWARD: 1,
};

// Combine into a single constants object
module.exports = Object.assign(
  {
    FILTERS,
    DEFAULT_FILTERS,
    DEFAULT_FILTERS_VALUES,
    FILTERBAR_DISPLAY_MODES: {
      NARROW: "narrow",
      WIDE: "wide",
    },
  },
  actionTypes,
  chromeRDPEnums,
  jstermCommands,
  prefs,
  historyCommands
);
