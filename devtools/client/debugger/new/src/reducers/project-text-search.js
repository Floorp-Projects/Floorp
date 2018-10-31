/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
// @format

/**
 * Project text search reducer
 * @module reducers/project-text-search
 */

import * as I from "immutable";
import makeRecord from "../utils/makeRecord";

import type { Action } from "../actions/types";
import type { Record } from "../utils/makeRecord";
import type { List } from "immutable";

export type Search = {
  id: string,
  filepath: string,
  matches: I.List<any>
};
export type StatusType = "INITIAL" | "FETCHING" | "DONE" | "ERROR";
export const statusType = {
  initial: "INITIAL",
  fetching: "FETCHING",
  done: "DONE",
  error: "ERROR"
};

export type ResultRecord = Record<Search>;
export type ResultList = List<ResultRecord>;
export type ProjectTextSearchState = {
  query: string,
  results: ResultList,
  status: string
};

export function initialProjectTextSearchState(): Record<
  ProjectTextSearchState
> {
  return makeRecord(
    ({
      query: "",
      results: I.List(),
      status: statusType.initial
    }: ProjectTextSearchState)
  )();
}

function update(
  state: Record<ProjectTextSearchState> = initialProjectTextSearchState(),
  action: Action
): Record<ProjectTextSearchState> {
  switch (action.type) {
    case "ADD_QUERY":
      const actionCopy = action;
      return state.update("query", value => actionCopy.query);

    case "CLEAR_QUERY":
      return state.merge({
        query: "",
        status: statusType.initial
      });

    case "ADD_SEARCH_RESULT":
      const results = state.get("results");
      return state.merge({ results: results.push(action.result) });

    case "UPDATE_STATUS":
      return state.merge({ status: action.status });

    case "CLEAR_SEARCH_RESULTS":
      return state.merge({
        results: state.get("results").clear()
      });

    case "CLEAR_SEARCH":
    case "CLOSE_PROJECT_SEARCH":
    case "NAVIGATE":
      return state.merge({
        query: "",
        results: state.get("results").clear(),
        status: statusType.initial
      });
  }
  return state;
}

type OuterState = { projectTextSearch: Record<ProjectTextSearchState> };

export function getTextSearchResults(state: OuterState) {
  return state.projectTextSearch.get("results");
}

export function getTextSearchStatus(state: OuterState) {
  return state.projectTextSearch.get("status");
}

export function getTextSearchQuery(state: OuterState) {
  return state.projectTextSearch.get("query");
}

export default update;
