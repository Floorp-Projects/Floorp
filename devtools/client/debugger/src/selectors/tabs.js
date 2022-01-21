/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { createSelector } from "reselect";
import { makeShallowQuery } from "../utils/resource";
import { getPrettySourceURL } from "../utils/source";

import { getSources, resourceAsSourceBase } from "../reducers/sources";

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
