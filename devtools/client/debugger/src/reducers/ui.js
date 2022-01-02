/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* eslint complexity: ["error", 35]*/

/**
 * UI reducer
 * @module reducers/ui
 */

import { prefs, features } from "../utils/prefs";

export const initialUIState = () => ({
  selectedPrimaryPaneTab: "sources",
  activeSearch: null,
  shownSource: null,
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
  sourceMapsEnabled: prefs.clientSourceMapsEnabled,
  javascriptEnabled: true,
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
      return { ...state, sourceMapsEnabled: action.value };
    }

    case "SET_ORIENTATION": {
      return { ...state, orientation: action.orientation };
    }

    case "SHOW_SOURCE": {
      return { ...state, shownSource: action.source };
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
      let lineRange = {};

      if (start && end && sourceId) {
        lineRange = { start, end, sourceId };
      }

      return { ...state, highlightedLineRange: lineRange };

    case "CLOSE_QUICK_OPEN":
    case "CLEAR_HIGHLIGHT_LINES":
      return { ...state, highlightedLineRange: {} };

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

    default: {
      return state;
    }
  }
}

// NOTE: we'd like to have the app state fully typed
// https://github.com/firefox-devtools/debugger/blob/master/src/reducers/sources.js#L179-L185

export function getSelectedPrimaryPaneTab(state) {
  return state.ui.selectedPrimaryPaneTab;
}

export function getActiveSearch(state) {
  return state.ui.activeSearch;
}

export function getFrameworkGroupingState(state) {
  return state.ui.frameworkGroupingOn;
}

export function getShownSource(state) {
  return state.ui.shownSource;
}

export function getPaneCollapse(state, position) {
  if (position == "start") {
    return state.ui.startPanelCollapsed;
  }

  return state.ui.endPanelCollapsed;
}

export function getHighlightedLineRange(state) {
  return state.ui.highlightedLineRange;
}

export function getConditionalPanelLocation(state) {
  return state.ui.conditionalPanelLocation;
}

export function getLogPointStatus(state) {
  return state.ui.isLogPoint;
}

export function getOrientation(state) {
  return state.ui.orientation;
}

export function getViewport(state) {
  return state.ui.viewport;
}

export function getCursorPosition(state) {
  return state.ui.cursorPosition;
}

export function getInlinePreview(state) {
  return state.ui.inlinePreviewEnabled;
}

export function getEditorWrapping(state) {
  return state.ui.editorWrappingEnabled;
}

export default update;
