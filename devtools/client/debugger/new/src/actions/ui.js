"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.setContextMenu = setContextMenu;
exports.setPrimaryPaneTab = setPrimaryPaneTab;
exports.closeActiveSearch = closeActiveSearch;
exports.setActiveSearch = setActiveSearch;
exports.toggleFrameworkGrouping = toggleFrameworkGrouping;
exports.showSource = showSource;
exports.togglePaneCollapse = togglePaneCollapse;
exports.highlightLineRange = highlightLineRange;
exports.flashLineRange = flashLineRange;
exports.clearHighlightLineRange = clearHighlightLineRange;
exports.openConditionalPanel = openConditionalPanel;
exports.closeConditionalPanel = closeConditionalPanel;
exports.clearProjectDirectoryRoot = clearProjectDirectoryRoot;
exports.setProjectDirectoryRoot = setProjectDirectoryRoot;
exports.setOrientation = setOrientation;

var _selectors = require("../selectors/index");

var _ui = require("../reducers/ui");

var _source = require("../utils/source");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function setContextMenu(type, event) {
  return ({
    dispatch
  }) => {
    dispatch({
      type: "SET_CONTEXT_MENU",
      contextMenu: {
        type,
        event
      }
    });
  };
}

function setPrimaryPaneTab(tabName) {
  return {
    type: "SET_PRIMARY_PANE_TAB",
    tabName
  };
}

function closeActiveSearch() {
  return {
    type: "TOGGLE_ACTIVE_SEARCH",
    value: null
  };
}

function setActiveSearch(activeSearch) {
  return ({
    dispatch,
    getState
  }) => {
    const activeSearchState = (0, _selectors.getActiveSearch)(getState());

    if (activeSearchState === activeSearch) {
      return;
    }

    if ((0, _selectors.getQuickOpenEnabled)(getState())) {
      dispatch({
        type: "CLOSE_QUICK_OPEN"
      });
    }

    dispatch({
      type: "TOGGLE_ACTIVE_SEARCH",
      value: activeSearch
    });
  };
}

function toggleFrameworkGrouping(toggleValue) {
  return ({
    dispatch,
    getState
  }) => {
    dispatch({
      type: "TOGGLE_FRAMEWORK_GROUPING",
      value: toggleValue
    });
  };
}

function showSource(sourceId) {
  return ({
    dispatch,
    getState
  }) => {
    const source = (0, _selectors.getSource)(getState(), sourceId);

    if (!source) {
      return;
    }

    if ((0, _selectors.getPaneCollapse)(getState(), "start")) {
      dispatch({
        type: "TOGGLE_PANE",
        position: "start",
        paneCollapsed: false
      });
    }

    dispatch(setPrimaryPaneTab("sources"));
    dispatch({
      type: "SHOW_SOURCE",
      sourceUrl: ""
    });
    dispatch({
      type: "SHOW_SOURCE",
      sourceUrl: (0, _source.getRawSourceURL)(source.url)
    });
  };
}

function togglePaneCollapse(position, paneCollapsed) {
  return ({
    dispatch,
    getState
  }) => {
    const prevPaneCollapse = (0, _selectors.getPaneCollapse)(getState(), position);

    if (prevPaneCollapse === paneCollapsed) {
      return;
    }

    dispatch({
      type: "TOGGLE_PANE",
      position,
      paneCollapsed
    });
  };
}
/**
 * @memberof actions/sources
 * @static
 */


function highlightLineRange(location) {
  return {
    type: "HIGHLIGHT_LINES",
    location
  };
}

function flashLineRange(location) {
  return ({
    dispatch
  }) => {
    dispatch(highlightLineRange(location));
    setTimeout(() => dispatch(clearHighlightLineRange()), 200);
  };
}
/**
 * @memberof actions/sources
 * @static
 */


function clearHighlightLineRange() {
  return {
    type: "CLEAR_HIGHLIGHT_LINES"
  };
}

function openConditionalPanel(line) {
  if (!line) {
    return;
  }

  return {
    type: "OPEN_CONDITIONAL_PANEL",
    line
  };
}

function closeConditionalPanel() {
  return {
    type: "CLOSE_CONDITIONAL_PANEL"
  };
}

function clearProjectDirectoryRoot() {
  return {
    type: "SET_PROJECT_DIRECTORY_ROOT",
    url: ""
  };
}

function setProjectDirectoryRoot(newRoot) {
  return ({
    dispatch,
    getState
  }) => {
    const curRoot = (0, _ui.getProjectDirectoryRoot)(getState());

    if (newRoot && curRoot) {
      const newRootArr = newRoot.replace(/\/+/g, "/").split("/");
      const curRootArr = curRoot.replace(/^\//, "").replace(/\/+/g, "/").split("/");

      if (newRootArr[0] !== curRootArr[0]) {
        newRootArr.splice(0, 2);
        newRoot = `${curRoot}/${newRootArr.join("/")}`;
      }
    }

    dispatch({
      type: "SET_PROJECT_DIRECTORY_ROOT",
      url: newRoot
    });
  };
}

function setOrientation(orientation) {
  return {
    type: "SET_ORIENTATION",
    orientation
  };
}