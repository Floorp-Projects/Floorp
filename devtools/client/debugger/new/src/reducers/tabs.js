"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.getSourcesForTabs = exports.getSourceTabs = exports.getTabs = undefined;
exports.removeSourceFromTabList = removeSourceFromTabList;
exports.removeSourcesFromTabList = removeSourcesFromTabList;
exports.getNewSelectedSourceId = getNewSelectedSourceId;

var _reselect = require("devtools/client/debugger/new/dist/vendors").vendored["reselect"];

var _devtoolsSourceMap = require("devtools/client/shared/source-map/index.js");

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
function isSimilarTab(tab, url, isOriginal) {
  return tab.url === url && tab.isOriginal === isOriginal;
}

function update(state = [], action) {
  switch (action.type) {
    case "ADD_TAB":
    case "UPDATE_TAB":
      return updateTabList(state, action);

    case "MOVE_TAB":
      return moveTabInList(state, action);

    case "CLOSE_TAB":
    case "CLOSE_TABS":
      _prefs.asyncStore.tabs = action.tabs;
      return action.tabs;

    default:
      return state;
  }
}

function removeSourceFromTabList(tabs, source) {
  return tabs.filter(tab => tab.url !== source.url || tab.isOriginal != (0, _devtoolsSourceMap.isOriginalId)(source.id));
}

function removeSourcesFromTabList(tabs, sources) {
  return sources.reduce((t, source) => removeSourceFromTabList(t, source), tabs);
}
/**
 * Adds the new source to the tab list if it is not already there
 * @memberof reducers/tabs
 * @static
 */


function updateTabList(tabs, {
  url,
  framework = null,
  isOriginal = false
}) {
  const currentIndex = tabs.findIndex(tab => isSimilarTab(tab, url, isOriginal));

  if (currentIndex === -1) {
    tabs = [{
      url,
      framework,
      isOriginal
    }, ...tabs];
  } else if (framework) {
    tabs[currentIndex].framework = framework;
  }

  _prefs.asyncStore.tabs = tabs;
  return tabs;
}

function moveTabInList(tabs, {
  url,
  tabIndex: newIndex
}) {
  const currentIndex = tabs.findIndex(tab => tab.url == url);
  tabs = (0, _lodashMove2.default)(tabs, currentIndex, newIndex);
  _prefs.asyncStore.tabs = tabs;
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

  const matchingTab = availableTabs.find(tab => isSimilarTab(tab, selectedTab.url, (0, _devtoolsSourceMap.isOriginalId)(selectedLocation.sourceId)));

  if (matchingTab) {
    const sources = state.sources.sources;

    if (!sources) {
      return "";
    }

    const selectedSource = (0, _sources.getSpecificSourceByURL)(state, selectedTab.url, (0, _devtoolsSourceMap.isOriginalId)(selectedTab.id));

    if (selectedSource) {
      return selectedSource.id;
    }

    return "";
  }

  const tabUrls = state.tabs.map(t => t.url);
  const leftNeighborIndex = Math.max(tabUrls.indexOf(selectedTab.url) - 1, 0);
  const lastAvailbleTabIndex = availableTabs.length - 1;
  const newSelectedTabIndex = Math.min(leftNeighborIndex, lastAvailbleTabIndex);
  const availableTab = availableTabs[newSelectedTabIndex];

  if (availableTab) {
    const tabSource = (0, _sources.getSpecificSourceByUrlInSources)((0, _sources.getSources)(state), (0, _sources.getUrls)(state), availableTab.url, availableTab.isOriginal);

    if (tabSource) {
      return tabSource.id;
    }
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

const getSourceTabs = exports.getSourceTabs = (0, _reselect.createSelector)(getTabs, _sources.getSources, _sources.getUrls, (tabs, sources, urls) => tabs.filter(tab => (0, _sources.getSpecificSourceByUrlInSources)(sources, urls, tab.url, tab.isOriginal)));
const getSourcesForTabs = exports.getSourcesForTabs = (0, _reselect.createSelector)(getSourceTabs, _sources.getSources, _sources.getUrls, (tabs, sources, urls) => {
  return tabs.map(tab => (0, _sources.getSpecificSourceByUrlInSources)(sources, urls, tab.url, tab.isOriginal)).filter(Boolean);
});
exports.default = update;