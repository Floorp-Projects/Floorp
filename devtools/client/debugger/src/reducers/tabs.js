/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Tabs reducer
 * @module reducers/tabs
 */

import { isOriginalId } from "devtools-source-map";

import { isSimilarTab, persistTabs } from "../utils/tabs";

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
      return updateTabList(state, action.source);

    case "MOVE_TAB":
      return moveTabInList(state, action);

    case "MOVE_TAB_BY_SOURCE_ID":
      return moveTabInListBySourceId(state, action);

    case "CLOSE_TAB":
      return removeSourceFromTabList(state, action);

    case "CLOSE_TABS":
      return removeSourcesFromTabList(state, action);

    case "ADD_SOURCES":
      return addVisibleTabs(state, action.sources);

    case "SET_SELECTED_LOCATION": {
      return addSelectedSource(state, action.source);
    }

    case "NAVIGATE": {
      return resetTabState(state);
    }

    case "REMOVE_THREAD": {
      return resetTabsForThread(state, action.threadActorID);
    }

    default:
      return state;
  }
}

function matchesSource(tab, source) {
  return tab.sourceId === source.id || matchesUrl(tab, source);
}

function matchesUrl(tab, source) {
  return tab.url === source.url && tab.isOriginal == isOriginalId(source.id);
}

function addSelectedSource(state, source) {
  if (state.tabs.some(({ sourceId }) => sourceId == source.id)) {
    return state;
  }

  return updateTabList(state, source);
}

function addVisibleTabs(state, sources) {
  const tabCount = state.tabs.filter(({ sourceId }) => sourceId).length;
  const tabs = state.tabs
    .map(tab => {
      const source = sources.find(src => matchesUrl(tab, src));
      if (!source) {
        return tab;
      }
      return { ...tab, sourceId: source.id, threadActorID: source.thread };
    })
    .filter(tab => tab.sourceId);

  if (tabs.length == tabCount) {
    return state;
  }

  return { tabs };
}

function removeSourceFromTabList(state, { source }) {
  const newTabs = state.tabs.filter(tab => !matchesSource(tab, source));
  if (newTabs.length == state.tabs.length) {
    return state;
  }
  return { tabs: newTabs };
}

function removeSourcesFromTabList(state, { sources }) {
  const newTabs = sources.reduce(
    (tabList, source) => tabList.filter(tab => !matchesSource(tab, source)),
    state.tabs
  );
  if (newTabs.length == state.tabs.length) {
    return state;
  }

  return { tabs: newTabs };
}

function resetTabsForThread(state, threadActorID) {
  const newTabs = state.tabs.filter(tab => tab.threadActorID !== threadActorID);
  if (newTabs.length == state.tabs.length) {
    return state;
  }
  return { tabs: newTabs };
}

/**
 * Adds the new source to the tab list if it is not already there.
 */
function updateTabList(state, source) {
  const { url } = source;
  const isOriginal = isOriginalId(source.id);

  let { tabs } = state;
  // Set currentIndex to -1 for URL-less tabs so that they aren't
  // filtered by isSimilarTab
  const currentIndex = url
    ? tabs.findIndex(tab => isSimilarTab(tab, url, isOriginal))
    : -1;

  if (currentIndex === -1) {
    const newTab = {
      url,
      sourceId: source.id,
      isOriginal,
      threadActorID: source.thread,
    };
    // New tabs are added first in the list
    tabs = [newTab, ...tabs];
  } else {
    return state;
  }

  return { ...state, tabs };
}

function moveTabInList(state, { url, tabIndex: newIndex }) {
  const { tabs } = state;
  const currentIndex = tabs.findIndex(tab => tab.url == url);
  return moveTab(tabs, currentIndex, newIndex);
}

function moveTabInListBySourceId(state, { sourceId, tabIndex: newIndex }) {
  const { tabs } = state;
  const currentIndex = tabs.findIndex(tab => tab.sourceId == sourceId);
  return moveTab(tabs, currentIndex, newIndex);
}

function moveTab(tabs, currentIndex, newIndex) {
  const item = tabs[currentIndex];

  const newTabs = Array.from(tabs);
  // Remove the item from its current location
  newTabs.splice(currentIndex, 1);
  // And add it to the new one
  newTabs.splice(newIndex, 0, item);

  return { tabs: newTabs };
}

export default update;
