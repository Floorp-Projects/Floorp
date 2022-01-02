/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  getActiveSearch,
  getPaneCollapse,
  getQuickOpenEnabled,
  getSource,
  getSourceContent,
  startsWithThreadActor,
  getFileSearchQuery,
  getProjectDirectoryRoot,
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

    dispatch({ type: "SHOW_SOURCE", source: null });
    dispatch(selectSource(cx, source.id));
    dispatch({ type: "SHOW_SOURCE", source });
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
    return;
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
    const threadActor = startsWithThreadActor(getState(), newRoot);

    let curRoot = getProjectDirectoryRoot(getState());

    // Remove the thread actor ID from the root path
    if (threadActor) {
      newRoot = newRoot.slice(threadActor.length + 1);
      curRoot = curRoot.slice(threadActor.length + 1);
    }

    if (newRoot && curRoot) {
      const newRootArr = newRoot.replace(/\/+/g, "/").split("/");
      const curRootArr = curRoot
        .replace(/^\//, "")
        .replace(/\/+/g, "/")
        .split("/");
      if (newRootArr[0] !== curRootArr[0]) {
        newRootArr.splice(0, 2);
        newRoot = `${curRoot}/${newRootArr.join("/")}`;
      }
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
