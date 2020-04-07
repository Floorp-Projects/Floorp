/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

/**
 * Quick Open reducer
 * @module reducers/quick-open
 */

import { parseQuickOpenQuery } from "../utils/quick-open";
import type { Action } from "../actions/types";

export type QuickOpenType = "sources" | "functions" | "goto" | "gotoSource";

export type QuickOpenState = {
  enabled: boolean,
  query: string,
  searchType: QuickOpenType,
};

export const createQuickOpenState = (): QuickOpenState => ({
  enabled: false,
  query: "",
  searchType: "sources",
});

export default function update(
  state: QuickOpenState = createQuickOpenState(),
  action: Action
): QuickOpenState {
  switch (action.type) {
    case "OPEN_QUICK_OPEN":
      if (action.query != null) {
        return {
          ...state,
          enabled: true,
          query: action.query,
          searchType: parseQuickOpenQuery(action.query),
        };
      }
      return { ...state, enabled: true };
    case "CLOSE_QUICK_OPEN":
      return createQuickOpenState();
    case "SET_QUICK_OPEN_QUERY":
      return {
        ...state,
        query: action.query,
        searchType: parseQuickOpenQuery(action.query),
      };
    default:
      return state;
  }
}

type OuterState = {
  quickOpen: QuickOpenState,
};

export function getQuickOpenEnabled(state: OuterState): boolean {
  return state.quickOpen.enabled;
}

export function getQuickOpenQuery(state: OuterState): string {
  return state.quickOpen.query;
}

export function getQuickOpenType(state: OuterState): QuickOpenType {
  return state.quickOpen.searchType;
}
