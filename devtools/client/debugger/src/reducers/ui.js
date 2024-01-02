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

export const initialUIState = () => ({
  selectedPrimaryPaneTab: "sources",
  activeSearch: null,
  startPanelCollapsed: prefs.startPanelCollapsed,
  endPanelCollapsed: prefs.endPanelCollapsed,
  frameworkGroupingOn: prefs.frameworkGroupingOn,

  // This is used from Outline's copy to clipboard context menu
  // and QuickOpen to highlight lines temporarily.
  // If defined, it will be an object with following attributes:
  // - sourceId, String
  // - start, Number, start line to highlight, 1-based
  // - end, Number, end line to highlight, 1-based
  highlightedLineRange: null,

  conditionalPanelLocation: null,
  isLogPoint: false,
  orientation: "horizontal",
  viewport: null,
  cursorPosition: null,
  inlinePreviewEnabled: features.inlinePreview,
  editorWrappingEnabled: prefs.editorWrapping,
  javascriptEnabled: true,
  javascriptTracingLogMethod: prefs.javascriptTracingLogMethod,
  javascriptTracingValues: prefs.javascriptTracingValues,
  javascriptTracingOnNextInteraction: prefs.javascriptTracingOnNextInteraction,
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
  projectSearchQuery: "",
  hideIgnoredSources: prefs.hideIgnoredSources,
  sourceMapIgnoreListEnabled: prefs.sourceMapIgnoreListEnabled,
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

    case "HIGHLIGHT_LINES": {
      return { ...state, highlightedLineRange: action.location };
    }

    case "CLOSE_QUICK_OPEN":
    case "CLEAR_HIGHLIGHT_LINES":
      if (!state.highlightedLineRange) {
        return state;
      }
      return { ...state, highlightedLineRange: null };

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
      return { ...state, highlightedLineRange: null };
    }

    case "REMOVE_THREAD": {
      // Reset the highlighted range if the related source has been removed
      const sourceId = state.highlightedLineRange?.sourceId;
      if (sourceId && action.sources.some(s => s.id == sourceId)) {
        return { ...state, highlightedLineRange: null };
      }
      return state;
    }

    case "SET_JAVASCRIPT_TRACING_LOG_METHOD": {
      prefs.javascriptTracingLogMethod = action.value;
      return { ...state, javascriptTracingLogMethod: action.value };
    }

    case "TOGGLE_JAVASCRIPT_TRACING_VALUES": {
      prefs.javascriptTracingValues = !prefs.javascriptTracingValues;
      return {
        ...state,
        javascriptTracingValues: prefs.javascriptTracingValues,
      };
    }

    case "TOGGLE_JAVASCRIPT_TRACING_ON_NEXT_INTERACTION": {
      prefs.javascriptTracingOnNextInteraction =
        !prefs.javascriptTracingOnNextInteraction;
      return {
        ...state,
        javascriptTracingOnNextInteraction:
          prefs.javascriptTracingOnNextInteraction,
      };
    }

    case "SET_SEARCH_OPTIONS": {
      state.mutableSearchOptions[action.searchKey] = {
        ...state.mutableSearchOptions[action.searchKey],
        ...action.searchOptions,
      };
      prefs.searchOptions = state.mutableSearchOptions;
      return { ...state };
    }

    case "SET_PROJECT_SEARCH_QUERY": {
      if (action.query != state.projectSearchQuery) {
        state.projectSearchQuery = action.query;
        return { ...state };
      }
      return state;
    }

    case "HIDE_IGNORED_SOURCES": {
      const { shouldHide } = action;
      if (shouldHide !== state.hideIgnoredSources) {
        prefs.hideIgnoredSources = shouldHide;
        return { ...state, hideIgnoredSources: shouldHide };
      }
      return state;
    }

    case "ENABLE_SOURCEMAP_IGNORELIST": {
      const { shouldEnable } = action;
      if (shouldEnable !== state.sourceMapIgnoreListEnabled) {
        prefs.sourceMapIgnoreListEnabled = shouldEnable;
        return { ...state, sourceMapIgnoreListEnabled: shouldEnable };
      }
      return state;
    }

    default: {
      return state;
    }
  }
}

export default update;
