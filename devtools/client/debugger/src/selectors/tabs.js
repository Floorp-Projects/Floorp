/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { createSelector } from "reselect";
import { getPrettySourceURL } from "../utils/source";

import { getSpecificSourceByURL } from "./sources";
import { isSimilarTab } from "../utils/tabs";

export const getTabs = state => state.tabs.tabs;

// Return the list of tabs which relates to an active source
export const getSourceTabs = createSelector(getTabs, tabs =>
  tabs.filter(tab => tab.source)
);

export const getSourcesForTabs = createSelector(getSourceTabs, sourceTabs => {
  return sourceTabs.map(tab => tab.source);
});

export function tabExists(state, sourceId) {
  return !!getSourceTabs(state).find(tab => tab.source.id == sourceId);
}

export function hasPrettyTab(state, source) {
  const prettyUrl = getPrettySourceURL(source.url);
  return getTabs(state).some(tab => tab.url === prettyUrl);
}

/**
 * Gets the next tab to select when a tab closes. Heuristics:
 * 1. if the selected tab is available, it remains selected
 * 2. if it is gone, the next available tab to the left should be active
 * 3. if the first tab is active and closed, select the second tab
 */
export function getNewSelectedSource(state, tabList) {
  const { selectedLocation } = state.sources;
  const availableTabs = getTabs(state);
  if (!selectedLocation) {
    return null;
  }

  const selectedSource = selectedLocation.source;
  if (!selectedSource) {
    return null;
  }

  const matchingTab = availableTabs.find(tab =>
    isSimilarTab(tab, selectedSource.url, selectedSource.isOriginal)
  );

  if (matchingTab) {
    const specificSelectedSource = getSpecificSourceByURL(
      state,
      selectedSource.url,
      selectedSource.isOriginal
    );

    if (specificSelectedSource) {
      return specificSelectedSource;
    }

    return null;
  }

  const tabUrls = tabList.map(tab => tab.url);
  const leftNeighborIndex = Math.max(
    tabUrls.indexOf(selectedSource.url) - 1,
    0
  );
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
      return tabSource;
    }
  }

  return null;
}
