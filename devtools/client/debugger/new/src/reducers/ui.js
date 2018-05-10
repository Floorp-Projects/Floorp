"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.createUIState = undefined;
exports.getSelectedPrimaryPaneTab = getSelectedPrimaryPaneTab;
exports.getActiveSearch = getActiveSearch;
exports.getContextMenu = getContextMenu;
exports.getFrameworkGroupingState = getFrameworkGroupingState;
exports.getShownSource = getShownSource;
exports.getPaneCollapse = getPaneCollapse;
exports.getHighlightedLineRange = getHighlightedLineRange;
exports.getConditionalPanelLine = getConditionalPanelLine;
exports.getProjectDirectoryRoot = getProjectDirectoryRoot;
exports.getOrientation = getOrientation;

var _makeRecord = require("../utils/makeRecord");

var _makeRecord2 = _interopRequireDefault(_makeRecord);

var _prefs = require("../utils/prefs");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * UI reducer
 * @module reducers/ui
 */
const createUIState = exports.createUIState = (0, _makeRecord2.default)({
  selectedPrimaryPaneTab: "sources",
  activeSearch: null,
  contextMenu: {},
  shownSource: "",
  projectDirectoryRoot: _prefs.prefs.projectDirectoryRoot,
  startPanelCollapsed: _prefs.prefs.startPanelCollapsed,
  endPanelCollapsed: _prefs.prefs.endPanelCollapsed,
  frameworkGroupingOn: _prefs.prefs.frameworkGroupingOn,
  highlightedLineRange: undefined,
  conditionalPanelLine: null,
  orientation: "horizontal"
});

function update(state = createUIState(), action) {
  switch (action.type) {
    case "TOGGLE_ACTIVE_SEARCH":
      {
        return state.set("activeSearch", action.value);
      }

    case "TOGGLE_FRAMEWORK_GROUPING":
      {
        _prefs.prefs.frameworkGroupingOn = action.value;
        return state.set("frameworkGroupingOn", action.value);
      }

    case "SET_CONTEXT_MENU":
      {
        return state.set("contextMenu", action.contextMenu);
      }

    case "SET_ORIENTATION":
      {
        return state.set("orientation", action.orientation);
      }

    case "SHOW_SOURCE":
      {
        return state.set("shownSource", action.sourceUrl);
      }

    case "TOGGLE_PANE":
      {
        if (action.position == "start") {
          _prefs.prefs.startPanelCollapsed = action.paneCollapsed;
          return state.set("startPanelCollapsed", action.paneCollapsed);
        }

        _prefs.prefs.endPanelCollapsed = action.paneCollapsed;
        return state.set("endPanelCollapsed", action.paneCollapsed);
      }

    case "HIGHLIGHT_LINES":
      const {
        start,
        end,
        sourceId
      } = action.location;
      let lineRange = {};

      if (start && end && sourceId) {
        lineRange = {
          start,
          end,
          sourceId
        };
      }

      return state.set("highlightedLineRange", lineRange);

    case "CLOSE_QUICK_OPEN":
    case "CLEAR_HIGHLIGHT_LINES":
      return state.set("highlightedLineRange", {});

    case "OPEN_CONDITIONAL_PANEL":
      return state.set("conditionalPanelLine", action.line);

    case "CLOSE_CONDITIONAL_PANEL":
      return state.set("conditionalPanelLine", null);

    case "SET_PROJECT_DIRECTORY_ROOT":
      _prefs.prefs.projectDirectoryRoot = action.url;
      return state.set("projectDirectoryRoot", action.url);

    case "SET_PRIMARY_PANE_TAB":
      return state.set("selectedPrimaryPaneTab", action.tabName);

    case "CLOSE_PROJECT_SEARCH":
      {
        if (state.get("activeSearch") === "project") {
          return state.set("activeSearch", null);
        }

        return state;
      }

    default:
      {
        return state;
      }
  }
} // NOTE: we'd like to have the app state fully typed
// https://github.com/devtools-html/debugger.html/blob/master/src/reducers/sources.js#L179-L185


function getSelectedPrimaryPaneTab(state) {
  return state.ui.get("selectedPrimaryPaneTab");
}

function getActiveSearch(state) {
  return state.ui.get("activeSearch");
}

function getContextMenu(state) {
  return state.ui.get("contextMenu");
}

function getFrameworkGroupingState(state) {
  return state.ui.get("frameworkGroupingOn");
}

function getShownSource(state) {
  return state.ui.get("shownSource");
}

function getPaneCollapse(state, position) {
  if (position == "start") {
    return state.ui.get("startPanelCollapsed");
  }

  return state.ui.get("endPanelCollapsed");
}

function getHighlightedLineRange(state) {
  return state.ui.get("highlightedLineRange");
}

function getConditionalPanelLine(state) {
  return state.ui.get("conditionalPanelLine");
}

function getProjectDirectoryRoot(state) {
  return state.ui.get("projectDirectoryRoot");
}

function getOrientation(state) {
  return state.ui.get("orientation");
}

exports.default = update;