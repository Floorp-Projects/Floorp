"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.getSourcesForTabs = exports.getSourceTabs = exports.getTabs = undefined;
exports.removeSourceFromTabList = removeSourceFromTabList;
exports.removeSourcesFromTabList = removeSourcesFromTabList;
exports.getNewSelectedSourceId = getNewSelectedSourceId;

var _reselect = require("devtools/client/debugger/new/dist/vendors").vendored["reselect"];

var _lodashMove = require("devtools/client/debugger/new/dist/vendors").vendored["lodash-move"];

var _lodashMove2 = _interopRequireDefault(_lodashMove);

var _prefs = require("../utils/prefs");

var _sources = require("./sources");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Tabs reducer
 * @module reducers/tabs
 */
function update(state = _prefs.prefs.tabs || [], action) {
  switch (action.type) {
    case "ADD_TAB":
      return updateTabList(state, action.url);

    case "MOVE_TAB":
      return updateTabList(state, action.url, action.tabIndex);

    case "CLOSE_TAB":
    case "CLOSE_TABS":
      _prefs.prefs.tabs = action.tabs;
      return action.tabs;

    default:
      return state;
  }
}

function removeSourceFromTabList(tabs, url) {
  return tabs.filter(tab => tab !== url);
}

function removeSourcesFromTabList(tabs, urls) {
  return urls.reduce((t, url) => removeSourceFromTabList(t, url), tabs);
}
/**
 * Adds the new source to the tab list if it is not already there
 * @memberof reducers/tabs
 * @static
 */


function updateTabList(tabs, url, newIndex) {
  const currentIndex = tabs.indexOf(url);

  if (currentIndex === -1) {
    tabs = [url, ...tabs];
  } else if (newIndex !== undefined) {
    tabs = (0, _lodashMove2.default)(tabs, currentIndex, newIndex);
  }

  _prefs.prefs.tabs = tabs;
  return tabs;
}
/**
 * Gets the next tab to select when a tab closes. Heuristics:
 * 1. if the selected tab is available, it remains selected
 * 2. if it is gone, the next available tab to the left should be active
 * 3. if the first tab is active and closed, select the second tab
 *
 * @memberof reducers/tabs
 * @static
 */


function getNewSelectedSourceId(state, availableTabs) {
  const selectedLocation = state.sources.selectedLocation;

  if (!selectedLocation) {
    return "";
  }

  const selectedTab = (0, _sources.getSource)(state, selectedLocation.sourceId);

  if (!selectedTab) {
    return "";
  }

  if (availableTabs.includes(selectedTab.url)) {
    const sources = state.sources.sources;

    if (!sources) {
      return "";
    }

    const selectedSource = (0, _sources.getSourceByURL)(state, selectedTab.url);

    if (selectedSource) {
      return selectedSource.id;
    }

    return "";
  }

  const tabUrls = state.tabs;
  const leftNeighborIndex = Math.max(tabUrls.indexOf(selectedTab.url) - 1, 0);
  const lastAvailbleTabIndex = availableTabs.length - 1;
  const newSelectedTabIndex = Math.min(leftNeighborIndex, lastAvailbleTabIndex);
  const availableTab = availableTabs[newSelectedTabIndex];
  const tabSource = (0, _sources.getSourceByUrlInSources)((0, _sources.getSources)(state), (0, _sources.getUrls)(state), availableTab);

  if (tabSource) {
    return tabSource.id;
  }

  return "";
} // Selectors
// Unfortunately, it's really hard to make these functions accept just
// the state that we care about and still type it with Flow. The
// problem is that we want to re-export all selectors from a single
// module for the UI, and all of those selectors should take the
// top-level app state, so we'd have to "wrap" them to automatically
// pick off the piece of state we're interested in. It's impossible
// (right now) to type those wrapped functions.


const getTabs = exports.getTabs = state => state.tabs;

const getSourceTabs = exports.getSourceTabs = (0, _reselect.createSelector)(getTabs, _sources.getSources, _sources.getUrls, (tabs, sources, urls) => tabs.filter(tab => (0, _sources.getSourceByUrlInSources)(sources, urls, tab)));
const getSourcesForTabs = exports.getSourcesForTabs = (0, _reselect.createSelector)(getSourceTabs, _sources.getSources, _sources.getUrls, (tabs, sources, urls) => {
  return tabs.map(tab => (0, _sources.getSourceByUrlInSources)(sources, urls, tab)).filter(source => source);
});
exports.default = update;