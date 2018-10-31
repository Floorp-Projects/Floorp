/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

/**
 * Redux actions for the search state
 * @module actions/search
 */

import { findSourceMatches } from "../workers/search";
import { getSource, hasPrettySource, getSourceList } from "../selectors";
import { isThirdParty } from "../utils/source";
import { loadSourceText } from "./sources/loadSourceText";
import { statusType } from "../reducers/project-text-search";

import type { Action, ThunkArgs } from "./types";

export function addSearchQuery(query: string): Action {
  return { type: "ADD_QUERY", query };
}

export function clearSearchQuery(): Action {
  return { type: "CLEAR_QUERY" };
}

export function addSearchResult(
  sourceId: string,
  filepath: string,
  matches: Object[]
): Action {
  return {
    type: "ADD_SEARCH_RESULT",
    result: { sourceId, filepath, matches }
  };
}

export function clearSearchResults(): Action {
  return { type: "CLEAR_SEARCH_RESULTS" };
}

export function clearSearch(): Action {
  return { type: "CLEAR_SEARCH" };
}

export function updateSearchStatus(status: string): Action {
  return { type: "UPDATE_STATUS", status };
}

export function closeProjectSearch(): Action {
  return { type: "CLOSE_PROJECT_SEARCH" };
}

export function searchSources(query: string) {
  return async ({ dispatch, getState }: ThunkArgs) => {
    await dispatch(clearSearchResults());
    await dispatch(addSearchQuery(query));
    dispatch(updateSearchStatus(statusType.fetching));
    const validSources = getSourceList(getState()).filter(
      source => !hasPrettySource(getState(), source.id) && !isThirdParty(source)
    );
    for (const source of validSources) {
      await dispatch(loadSourceText(source));
      await dispatch(searchSource(source.id, query));
    }
    dispatch(updateSearchStatus(statusType.done));
  };
}

export function searchSource(sourceId: string, query: string) {
  return async ({ dispatch, getState }: ThunkArgs) => {
    const source = getSource(getState(), sourceId);
    if (!source) {
      return;
    }

    const matches = await findSourceMatches(source, query);
    if (!matches.length) {
      return;
    }
    dispatch(addSearchResult(source.id, source.url, matches));
  };
}
