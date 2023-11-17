/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { getSelectedSource } from "./sources";

export function getSelectedPrimaryPaneTab(state) {
  return state.ui.selectedPrimaryPaneTab;
}

export function getActiveSearch(state) {
  return state.ui.activeSearch;
}

export function getFrameworkGroupingState(state) {
  return state.ui.frameworkGroupingOn;
}

export function getPaneCollapse(state, position) {
  if (position == "start") {
    return state.ui.startPanelCollapsed;
  }

  return state.ui.endPanelCollapsed;
}

export function getHighlightedLineRangeForSelectedSource(state) {
  const selectedSource = getSelectedSource(state);
  if (!selectedSource) {
    return null;
  }
  // Only return the highlighted line range if it matches the selected source
  const highlightedLineRange = state.ui.highlightedLineRange;
  if (
    highlightedLineRange &&
    selectedSource.id == highlightedLineRange.sourceId
  ) {
    return highlightedLineRange;
  }
  return null;
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

export function getJavascriptTracingLogMethod(state) {
  return state.ui.javascriptTracingLogMethod;
}

export function getSearchOptions(state, searchKey) {
  return state.ui.mutableSearchOptions[searchKey];
}

export function getProjectSearchQuery(state) {
  return state.ui.projectSearchQuery;
}

export function getHideIgnoredSources(state) {
  return state.ui.hideIgnoredSources;
}

export function isSourceMapIgnoreListEnabled(state) {
  return state.ui.sourceMapIgnoreListEnabled;
}

export function supportsDebuggerStatementIgnore(state) {
  return state.ui.supportsDebuggerStatementIgnore;
}
