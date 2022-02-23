/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Tabs reducer
 * @module reducers/tabs
 */

import { createSelector } from "reselect";
import { isOriginalId } from "devtools-source-map";
import move from "lodash-move";

import { isSimilarTab, persistTabs } from "../utils/tabs";
import { makeShallowQuery } from "../utils/resource";
import { getPrettySourceURL } from "../utils/source";

import {
  getSource,
  getSpecificSourceByURL,
  getSources,
  resourceAsSourceBase,
} from "./sources";

export function initialTabState() {
  return { tabs: [] };
}

function resetTabState(state) {
  const tabs = persistTabs(state.tabs);
  return { tabs };
}

function update(state = initialTabState(), action) {
  switch (action.type) {
    case "ADD_TAB":
    case "UPDATE_TAB":
      return updateTabList(state, action);

    case "MOVE_TAB":
      return moveTabInList(state, action);
    case "MOVE_TAB_BY_SOURCE_ID":
      return moveTabInListBySourceId(state, action);

    case "CLOSE_TAB":
      return removeSourceFromTabList(state, action);

    case "CLOSE_TABS":
      return removeSourcesFromTabList(state, action);

    case "ADD_SOURCE":
      return addVisibleTabs(state, [action.source]);

    case "ADD_SOURCES":
      return addVisibleTabs(state, action.sources);

    case "SET_SELECTED_LOCATION": {
      return addSelectedSource(state, action.source);
    }

    case "NAVIGATE": {
      return resetTabState(state);
    }

    default:
      return state;
  }
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
export function getNewSelectedSourceId(state, tabList) {
  const { selectedLocation } = state.sources;
  const availableTabs = state.tabs.tabs;
  if (!selectedLocation) {
    return "";
  }

  const selectedTab = getSource(state, selectedLocation.sourceId);
  if (!selectedTab) {
    return "";
  }

  const matchingTab = availableTabs.find(tab =>
    isSimilarTab(tab, selectedTab.url, isOriginalId(selectedLocation.sourceId))
  );

  if (matchingTab) {
    const { sources } = state.sources;
    if (!sources) {
      return "";
    }

    const selectedSource = getSpecificSourceByURL(
      state,
      selectedTab.url,
      selectedTab.isOriginal
    );

    if (selectedSource) {
      return selectedSource.id;
    }

    return "";
  }

  const tabUrls = tabList.map(tab => tab.url);
  const leftNeighborIndex = Math.max(tabUrls.indexOf(selectedTab.url) - 1, 0);
  const lastAvailbleTabIndex = availableTabs.length - 1;
  const newSelectedTabIndex = Math.min(leftNeighborIndex, lastAvailbleTabIndex);
  const availableTab = availableTabs[newSelectedTabIndex];

  if (availableTab) {
    const tabSource = getSpecificSourceByURL(
      state,
      availableTab.url,
      availableTab.isOriginal
    );

    if (tabSource) {
      return tabSource.id;
    }
  }

  return "";
}

function matchesSource(tab, source) {
  return tab.sourceId === source.id || matchesUrl(tab, source);
}

function matchesUrl(tab, source) {
  return tab.url === source.url && tab.isOriginal == isOriginalId(source.id);
}

function addSelectedSource(state, source) {
  if (
    state.tabs
      .filter(({ sourceId }) => sourceId)
      .map(({ sourceId }) => sourceId)
      .includes(source.id)
  ) {
    return state;
  }

  const isOriginal = isOriginalId(source.id);
  return updateTabList(state, {
    url: source.url,
    isOriginal,
    framework: null,
    sourceId: source.id,
  });
}

function addVisibleTabs(state, sources) {
  const tabCount = state.tabs.filter(({ sourceId }) => sourceId).length;
  const tabs = state.tabs
    .map(tab => {
      const source = sources.find(src => matchesUrl(tab, src));
      if (!source) {
        return tab;
      }
      return { ...tab, sourceId: source.id };
    })
    .filter(tab => tab.sourceId);

  if (tabs.length == tabCount) {
    return state;
  }

  return { tabs };
}

function removeSourceFromTabList(state, { source }) {
  const { tabs } = state;
  const newTabs = tabs.filter(tab => !matchesSource(tab, source));
  return { tabs: newTabs };
}

function removeSourcesFromTabList(state, { sources }) {
  const { tabs } = state;

  const newTabs = sources.reduce(
    (tabList, source) => tabList.filter(tab => !matchesSource(tab, source)),
    tabs
  );

  return { tabs: newTabs };
}

/**
 * Adds the new source to the tab list if it is not already there
 * @memberof reducers/tabs
 * @static
 */
function updateTabList(
  state,
  { url, framework = null, sourceId, isOriginal = false }
) {
  let { tabs } = state;
  // Set currentIndex to -1 for URL-less tabs so that they aren't
  // filtered by isSimilarTab
  const currentIndex = url
    ? tabs.findIndex(tab => isSimilarTab(tab, url, isOriginal))
    : -1;

  if (currentIndex === -1) {
    const newTab = {
      url,
      framework,
      sourceId,
      isOriginal,
    };
    tabs = [newTab, ...tabs];
  } else if (framework) {
    tabs[currentIndex].framework = framework;
  }

  return { ...state, tabs };
}

function moveTabInList(state, { url, tabIndex: newIndex }) {
  let { tabs } = state;
  const currentIndex = tabs.findIndex(tab => tab.url == url);
  tabs = move(tabs, currentIndex, newIndex);
  return { tabs };
}

function moveTabInListBySourceId(state, { sourceId, tabIndex: newIndex }) {
  let { tabs } = state;
  const currentIndex = tabs.findIndex(tab => tab.sourceId == sourceId);
  tabs = move(tabs, currentIndex, newIndex);
  return { tabs };
}

// Selectors

export const getTabs = state => state.tabs.tabs;

export const getSourceTabs = createSelector(
  state => state.tabs,
  ({ tabs }) => tabs.filter(tab => tab.sourceId)
);

export const getSourcesForTabs = state => {
  const tabs = getSourceTabs(state);
  const sources = getSources(state);
  return querySourcesForTabs(sources, tabs);
};

const querySourcesForTabs = makeShallowQuery({
  filter: (_, tabs) => tabs.map(({ sourceId }) => sourceId),
  map: resourceAsSourceBase,
  reduce: items => items,
});

export function tabExists(state, sourceId) {
  return !!getSourceTabs(state).find(tab => tab.sourceId == sourceId);
}

export function hasPrettyTab(state, sourceUrl) {
  const prettyUrl = getPrettySourceURL(sourceUrl);
  return !!getSourceTabs(state).find(tab => tab.url === prettyUrl);
}

export default update;
