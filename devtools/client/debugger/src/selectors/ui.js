/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

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
