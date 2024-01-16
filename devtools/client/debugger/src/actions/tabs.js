/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Redux actions for the editor tabs
 */

import { removeDocument } from "../utils/editor";
import { selectSource } from "./sources";

import { getSelectedLocation, getSourcesForTabs } from "../selectors";

export function addTab(source, sourceActor) {
  return {
    type: "ADD_TAB",
    source,
    sourceActor,
  };
}

export function moveTab(url, tabIndex) {
  return {
    type: "MOVE_TAB",
    url,
    tabIndex,
  };
}

export function moveTabBySourceId(sourceId, tabIndex) {
  return {
    type: "MOVE_TAB_BY_SOURCE_ID",
    sourceId,
    tabIndex,
  };
}

export function closeTab(source) {
  return closeTabs([source]);
}

export function closeTabs(sources) {
  return ({ dispatch, getState }) => {
    if (!sources.length) {
      return;
    }

    for (const source of sources) {
      removeDocument(source.id);
    }

    // If we are removing the tabs for the selected location,
    // we need to select another source
    const newSourceToSelect = getNewSourceToSelect(getState(), sources);

    dispatch({ type: "CLOSE_TABS", sources });

    dispatch(selectSource(newSourceToSelect));
  };
}

/**
 * Compute the potential new source to select while closing tabs for a given set of sources.
 *
 * @param {Object} state
 *        Redux state object.
 * @param {Array<Source>} closedTabsSources
 *        Ordered list of source object for which tabs should be closed.
 *        Should be a consecutive list of source matching the order of tabs reducer.
 */
function getNewSourceToSelect(state, closedTabsSources) {
  const selectedLocation = getSelectedLocation(state);
  // Do not try to select any source if none was selected before
  if (!selectedLocation) {
    return null;
  }
  // Keep selecting the same source if we aren't removing the currently selected source
  if (!closedTabsSources.includes(selectedLocation.source)) {
    return selectedLocation.source;
  }
  const tabsSources = getSourcesForTabs(state);
  // Assume that `sources` is a consecutive list of tab's sources
  // ordered in the same way as `tabsSources`.
  const lastRemovedTabSource = closedTabsSources.at(-1);
  const lastRemovedTabIndex = tabsSources.indexOf(lastRemovedTabSource);
  if (lastRemovedTabIndex == -1) {
    // This is unexpected, do not try to select any source.
    return null;
  }
  // If there is some tabs after the last removed tab, select the first one.
  if (lastRemovedTabIndex + 1 < tabsSources.length) {
    return tabsSources[lastRemovedTabIndex + 1];
  }

  // If there is some tabs before the first removed tab, select the last one.
  const firstRemovedTabIndex =
    lastRemovedTabIndex - (closedTabsSources.length - 1);
  if (firstRemovedTabIndex > 0) {
    return tabsSources[firstRemovedTabIndex - 1];
  }

  // It looks like we removed all the tabs
  return null;
}
