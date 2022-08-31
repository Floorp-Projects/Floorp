/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  getActiveSearch,
  getPaneCollapse,
  getQuickOpenEnabled,
  getSource,
  getSourceContent,
  getFileSearchQuery,
  getMainThread,
} from "../selectors";
import { selectSource } from "../actions/sources/select";
import {
  getEditor,
  getLocationsInViewport,
  updateDocuments,
} from "../utils/editor";
import { searchContents } from "./file-search";
import { copyToTheClipboard } from "../utils/clipboard";
import { isFulfilled } from "../utils/async-value";

export function setPrimaryPaneTab(tabName) {
  return { type: "SET_PRIMARY_PANE_TAB", tabName };
}

export function closeActiveSearch() {
  return {
    type: "TOGGLE_ACTIVE_SEARCH",
    value: null,
  };
}

export function setActiveSearch(activeSearch) {
  return ({ dispatch, getState }) => {
    const activeSearchState = getActiveSearch(getState());
    if (activeSearchState === activeSearch) {
      return;
    }

    if (getQuickOpenEnabled(getState())) {
      dispatch({ type: "CLOSE_QUICK_OPEN" });
    }

    dispatch({
      type: "TOGGLE_ACTIVE_SEARCH",
      value: activeSearch,
    });
  };
}

export function updateActiveFileSearch(cx) {
  return ({ dispatch, getState }) => {
    const isFileSearchOpen = getActiveSearch(getState()) === "file";
    const fileSearchQuery = getFileSearchQuery(getState());
    if (isFileSearchOpen && fileSearchQuery) {
      const editor = getEditor();
      dispatch(searchContents(cx, fileSearchQuery, editor, false));
    }
  };
}

export function toggleFrameworkGrouping(toggleValue) {
  return ({ dispatch, getState }) => {
    dispatch({
      type: "TOGGLE_FRAMEWORK_GROUPING",
      value: toggleValue,
    });
  };
}

export function toggleInlinePreview(toggleValue) {
  return ({ dispatch, getState }) => {
    dispatch({
      type: "TOGGLE_INLINE_PREVIEW",
      value: toggleValue,
    });
  };
}

export function toggleEditorWrapping(toggleValue) {
  return ({ dispatch, getState }) => {
    updateDocuments(doc => doc.cm.setOption("lineWrapping", toggleValue));

    dispatch({
      type: "TOGGLE_EDITOR_WRAPPING",
      value: toggleValue,
    });
  };
}

export function toggleSourceMapsEnabled(toggleValue) {
  return ({ dispatch, getState }) => {
    dispatch({
      type: "TOGGLE_SOURCE_MAPS_ENABLED",
      value: toggleValue,
    });
  };
}

export function showSource(cx, sourceId) {
  return ({ dispatch, getState }) => {
    const source = getSource(getState(), sourceId);
    if (!source) {
      return;
    }

    if (getPaneCollapse(getState(), "start")) {
      dispatch({
        type: "TOGGLE_PANE",
        position: "start",
        paneCollapsed: false,
      });
    }

    dispatch(setPrimaryPaneTab("sources"));

    dispatch(selectSource(cx, source.id));
  };
}

export function togglePaneCollapse(position, paneCollapsed) {
  return ({ dispatch, getState }) => {
    const prevPaneCollapse = getPaneCollapse(getState(), position);
    if (prevPaneCollapse === paneCollapsed) {
      return;
    }

    dispatch({
      type: "TOGGLE_PANE",
      position,
      paneCollapsed,
    });
  };
}

/**
 * @memberof actions/sources
 * @static
 */
export function highlightLineRange(location) {
  return {
    type: "HIGHLIGHT_LINES",
    location,
  };
}

export function flashLineRange(location) {
  return ({ dispatch }) => {
    dispatch(highlightLineRange(location));
    setTimeout(() => dispatch(clearHighlightLineRange()), 200);
  };
}

/**
 * @memberof actions/sources
 * @static
 */
export function clearHighlightLineRange() {
  return {
    type: "CLEAR_HIGHLIGHT_LINES",
  };
}

export function openConditionalPanel(location, log = false) {
  if (!location) {
    return null;
  }

  return {
    type: "OPEN_CONDITIONAL_PANEL",
    location,
    log,
  };
}

export function closeConditionalPanel() {
  return {
    type: "CLOSE_CONDITIONAL_PANEL",
  };
}

export function clearProjectDirectoryRoot(cx) {
  return {
    type: "SET_PROJECT_DIRECTORY_ROOT",
    cx,
    url: "",
    name: "",
  };
}

export function setProjectDirectoryRoot(cx, newRoot, newName) {
  return ({ dispatch, getState }) => {
    // If the new project root is against the top level thread,
    // replace its thread ID with "top-level", so that later,
    // getDirectoryForUniquePath could match the project root,
    // even after a page reload where the new top level thread actor ID
    // will be different.
    const mainThread = getMainThread(getState());
    if (mainThread && newRoot.startsWith(mainThread.actor)) {
      newRoot = newRoot.replace(mainThread.actor, "top-level");
    }
    dispatch({
      type: "SET_PROJECT_DIRECTORY_ROOT",
      cx,
      url: newRoot,
      name: newName,
    });
  };
}

export function updateViewport() {
  return {
    type: "SET_VIEWPORT",
    viewport: getLocationsInViewport(getEditor()),
  };
}

export function updateCursorPosition(cursorPosition) {
  return { type: "SET_CURSOR_POSITION", cursorPosition };
}

export function setOrientation(orientation) {
  return { type: "SET_ORIENTATION", orientation };
}

export function copyToClipboard(source) {
  return ({ dispatch, getState }) => {
    const content = getSourceContent(getState(), source.id);
    if (content && isFulfilled(content) && content.value.type === "text") {
      copyToTheClipboard(content.value.value);
    }
  };
}
