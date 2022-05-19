/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { createSelector } from "reselect";
import { shallowEqual } from "../utils/shallow-equal";
import { getPrettySourceURL } from "../utils/source";

import {
  getLocationSource,
  getSpecificSourceByURL,
  getSourcesMap,
} from "./sources";
import { isOriginalId } from "devtools-source-map";
import { isSimilarTab } from "../utils/tabs";

export const getTabs = state => state.tabs.tabs;

export const getSourceTabs = createSelector(
  state => state.tabs,
  ({ tabs }) => tabs.filter(tab => tab.sourceId)
);

export const getSourcesForTabs = createSelector(
  getSourcesMap,
  getSourceTabs,
  (sourcesMap, sourceTabs) => {
    return sourceTabs.map(tab => sourcesMap.get(tab.sourceId));
  },
  { equalityCheck: shallowEqual, resultEqualityCheck: shallowEqual }
);

export function tabExists(state, sourceId) {
  return !!getSourceTabs(state).find(tab => tab.sourceId == sourceId);
}

export function hasPrettyTab(state, sourceUrl) {
  const prettyUrl = getPrettySourceURL(sourceUrl);
  return !!getSourceTabs(state).find(tab => tab.url === prettyUrl);
}

/**
 * Gets the next tab to select when a tab closes. Heuristics:
 * 1. if the selected tab is available, it remains selected
 * 2. if it is gone, the next available tab to the left should be active
 * 3. if the first tab is active and closed, select the second tab
 */
export function getNewSelectedSourceId(state, tabList) {
  const { selectedLocation } = state.sources;
  const availableTabs = state.tabs.tabs;
  if (!selectedLocation) {
    return "";
  }

  const selectedTab = getLocationSource(state, selectedLocation);
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
