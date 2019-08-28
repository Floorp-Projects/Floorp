/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

/**
 * UI reducer
 * @module reducers/ui
 */

import { prefs, features } from "../utils/prefs";

import type { Source, Range, SourceLocation } from "../types";

import type { Action, panelPositionType } from "../actions/types";

export type ActiveSearchType = "project" | "file";

export type OrientationType = "horizontal" | "vertical";

export type SelectedPrimaryPaneTabType = "sources" | "outline";

export type UIState = {
  selectedPrimaryPaneTab: SelectedPrimaryPaneTabType,
  activeSearch: ?ActiveSearchType,
  shownSource: ?Source,
  startPanelCollapsed: boolean,
  endPanelCollapsed: boolean,
  frameworkGroupingOn: boolean,
  orientation: OrientationType,
  viewport: ?Range,
  highlightedLineRange?: {
    start?: number,
    end?: number,
    sourceId?: number,
  },
  conditionalPanelLocation: null | SourceLocation,
  isLogPoint: boolean,
  inlinePreviewEnabled: boolean,
};

export const createUIState = (): UIState => ({
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
  inlinePreviewEnabled: features.inlinePreview,
});

function update(state: UIState = createUIState(), action: Action): UIState {
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
type OuterState = { ui: UIState };

export function getSelectedPrimaryPaneTab(
  state: OuterState
): SelectedPrimaryPaneTabType {
  return state.ui.selectedPrimaryPaneTab;
}

export function getActiveSearch(state: OuterState): ?ActiveSearchType {
  return state.ui.activeSearch;
}

export function getFrameworkGroupingState(state: OuterState): boolean {
  return state.ui.frameworkGroupingOn;
}

export function getShownSource(state: OuterState): ?Source {
  return state.ui.shownSource;
}

export function getPaneCollapse(
  state: OuterState,
  position: panelPositionType
): boolean {
  if (position == "start") {
    return state.ui.startPanelCollapsed;
  }

  return state.ui.endPanelCollapsed;
}

export function getHighlightedLineRange(state: OuterState) {
  return state.ui.highlightedLineRange;
}

export function getConditionalPanelLocation(
  state: OuterState
): null | SourceLocation {
  return state.ui.conditionalPanelLocation;
}

export function getLogPointStatus(state: OuterState): boolean {
  return state.ui.isLogPoint;
}

export function getOrientation(state: OuterState): OrientationType {
  return state.ui.orientation;
}

export function getViewport(state: OuterState) {
  return state.ui.viewport;
}

export function getInlinePreview(state: OuterState) {
  return state.ui.inlinePreviewEnabled;
}

export default update;
