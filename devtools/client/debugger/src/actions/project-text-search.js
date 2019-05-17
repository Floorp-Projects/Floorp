/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

/**
 * Redux actions for the search state
 * @module actions/search
 */

import { isFulfilled } from "../utils/async-value";
import { findSourceMatches } from "../workers/search";
import {
  getSource,
  hasPrettySource,
  getSourceList,
  getSourceContent,
} from "../selectors";
import { isThirdParty } from "../utils/source";
import { loadSourceText } from "./sources/loadSourceText";
import {
  statusType,
  getTextSearchOperation,
  getTextSearchStatus,
} from "../reducers/project-text-search";

import type { Action, ThunkArgs } from "./types";
import type { Context } from "../types";
import type { SearchOperation } from "../reducers/project-text-search";

export function addSearchQuery(cx: Context, query: string): Action {
  return { type: "ADD_QUERY", cx, query };
}

export function addOngoingSearch(
  cx: Context,
  ongoingSearch: SearchOperation
): Action {
  return { type: "ADD_ONGOING_SEARCH", cx, ongoingSearch };
}

export function addSearchResult(
  cx: Context,
  sourceId: string,
  filepath: string,
  matches: Object[]
): Action {
  return {
    type: "ADD_SEARCH_RESULT",
    cx,
    result: { sourceId, filepath, matches },
  };
}

export function clearSearchResults(cx: Context): Action {
  return { type: "CLEAR_SEARCH_RESULTS", cx };
}

export function clearSearch(cx: Context): Action {
  return { type: "CLEAR_SEARCH", cx };
}

export function updateSearchStatus(cx: Context, status: string): Action {
  return { type: "UPDATE_STATUS", cx, status };
}

export function closeProjectSearch(cx: Context) {
  return ({ dispatch, getState }: ThunkArgs) => {
    dispatch(stopOngoingSearch(cx));
    dispatch({ type: "CLOSE_PROJECT_SEARCH" });
  };
}

export function stopOngoingSearch(cx: Context) {
  return ({ dispatch, getState }: ThunkArgs) => {
    const state = getState();
    const ongoingSearch = getTextSearchOperation(state);
    const status = getTextSearchStatus(state);
    if (ongoingSearch && status !== statusType.done) {
      ongoingSearch.cancel();
      dispatch(updateSearchStatus(cx, statusType.cancelled));
    }
  };
}

export function searchSources(cx: Context, query: string) {
  let cancelled = false;

  const search = async ({ dispatch, getState }: ThunkArgs) => {
    dispatch(stopOngoingSearch(cx));
    await dispatch(addOngoingSearch(cx, search));
    await dispatch(clearSearchResults(cx));
    await dispatch(addSearchQuery(cx, query));
    dispatch(updateSearchStatus(cx, statusType.fetching));
    const validSources = getSourceList(getState()).filter(
      source => !hasPrettySource(getState(), source.id) && !isThirdParty(source)
    );
    for (const source of validSources) {
      if (cancelled) {
        return;
      }
      await dispatch(loadSourceText({ cx, source }));
      await dispatch(searchSource(cx, source.id, query));
    }
    dispatch(updateSearchStatus(cx, statusType.done));
  };

  search.cancel = () => {
    cancelled = true;
  };

  return search;
}

export function searchSource(cx: Context, sourceId: string, query: string) {
  return async ({ dispatch, getState }: ThunkArgs) => {
    const source = getSource(getState(), sourceId);
    if (!source) {
      return;
    }

    const content = getSourceContent(getState(), source.id);
    let matches = [];
    if (content && isFulfilled(content) && content.value.type === "text") {
      matches = await findSourceMatches(source.id, content.value, query);
    }
    if (!matches.length) {
      return;
    }
    dispatch(addSearchResult(cx, source.id, source.url, matches));
  };
}
