/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* eslint complexity: ["error", 35]*/

/**
 * UI reducer
 * @module reducers/ui
 */

import { prefs, features } from "../utils/prefs";
import { searchKeys } from "../constants";

export const initialUIState = ({ supportsJavascriptTracing = false } = {}) => ({
  selectedPrimaryPaneTab: "sources",
  activeSearch: null,
  startPanelCollapsed: prefs.startPanelCollapsed,
  endPanelCollapsed: prefs.endPanelCollapsed,
  frameworkGroupingOn: prefs.frameworkGroupingOn,
  highlightedLineRange: undefined,
  conditionalPanelLocation: null,
  isLogPoint: false,
  orientation: "horizontal",
  viewport: null,
  cursorPosition: null,
  inlinePreviewEnabled: features.inlinePreview,
  editorWrappingEnabled: prefs.editorWrapping,
  javascriptEnabled: true,
  supportsJavascriptTracing,
  javascriptTracingLogMethod: prefs.javascriptTracingLogMethod,
  mutableSearchOptions: prefs.searchOptions || {
    [searchKeys.FILE_SEARCH]: {
      regexMatch: false,
      wholeWord: false,
      caseSensitive: false,
      excludePatterns: "",
    },
    [searchKeys.PROJECT_SEARCH]: {
      regexMatch: false,
      wholeWord: false,
      caseSensitive: false,
      excludePatterns: "",
    },
    [searchKeys.QUICKOPEN_SEARCH]: {
      regexMatch: false,
      wholeWord: false,
      caseSensitive: false,
      excludePatterns: "",
    },
  },
});

function update(state = initialUIState(), action) {
  switch (action.type) {
    case "TOGGLE_ACTIVE_SEARCH": {
      return { ...state, activeSearch: action.value };
    }

    case "TOGGLE_FRAMEWORK_GROUPING": {
      prefs.frameworkGroupingOn = action.value;
      return { ...state, frameworkGroupingOn: action.value };
    }

    case "TOGGLE_INLINE_PREVIEW": {
      features.inlinePreview = action.value;
      return { ...state, inlinePreviewEnabled: action.value };
    }

    case "TOGGLE_EDITOR_WRAPPING": {
      prefs.editorWrapping = action.value;
      return { ...state, editorWrappingEnabled: action.value };
    }

    case "TOGGLE_JAVASCRIPT_ENABLED": {
      return { ...state, javascriptEnabled: action.value };
    }

    case "TOGGLE_SOURCE_MAPS_ENABLED": {
      prefs.clientSourceMapsEnabled = action.value;
      return { ...state };
    }

    case "SET_ORIENTATION": {
      return { ...state, orientation: action.orientation };
    }

    case "TOGGLE_PANE": {
      if (action.position == "start") {
        prefs.startPanelCollapsed = action.paneCollapsed;
        return { ...state, startPanelCollapsed: action.paneCollapsed };
      }

      prefs.endPanelCollapsed = action.paneCollapsed;
      return { ...state, endPanelCollapsed: action.paneCollapsed };
    }

    case "HIGHLIGHT_LINES":
      const { start, end, sourceId } = action.location;
      let lineRange;

      // Lines are one-based so the check below is fine.
      if (start && end && sourceId) {
        lineRange = { start, end, sourceId };
      }

      return { ...state, highlightedLineRange: lineRange };

    case "CLOSE_QUICK_OPEN":
    case "CLEAR_HIGHLIGHT_LINES":
      return { ...state, highlightedLineRange: undefined };

    case "OPEN_CONDITIONAL_PANEL":
      return {
        ...state,
        conditionalPanelLocation: action.location,
        isLogPoint: action.log,
      };

    case "CLOSE_CONDITIONAL_PANEL":
      return { ...state, conditionalPanelLocation: null };

    case "SET_PRIMARY_PANE_TAB":
      return { ...state, selectedPrimaryPaneTab: action.tabName };

    case "CLOSE_PROJECT_SEARCH": {
      if (state.activeSearch === "project") {
        return { ...state, activeSearch: null };
      }
      return state;
    }

    case "SET_VIEWPORT": {
      return { ...state, viewport: action.viewport };
    }

    case "SET_CURSOR_POSITION": {
      return { ...state, cursorPosition: action.cursorPosition };
    }

    case "NAVIGATE": {
      return { ...state, activeSearch: null, highlightedLineRange: {} };
    }

    case "SET_JAVASCRIPT_TRACING_LOG_METHOD": {
      prefs.javascriptTracingLogMethod = action.value;
      return { ...state, javascriptTracingLogMethod: action.value };
    }

    case "SET_SEARCH_OPTIONS": {
      state.mutableSearchOptions[action.searchKey] = {
        ...state.mutableSearchOptions[action.searchKey],
        ...action.searchOptions,
      };
      prefs.searchOptions = state.mutableSearchOptions;
      return { ...state };
    }

    default: {
      return state;
    }
  }
}

export default update;
