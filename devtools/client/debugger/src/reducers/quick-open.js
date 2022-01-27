/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Quick Open reducer
 * @module reducers/quick-open
 */

import { parseQuickOpenQuery } from "../utils/quick-open";

export const initialQuickOpenState = () => ({
  enabled: false,
  query: "",
  searchType: "sources",
});

export default function update(state = initialQuickOpenState(), action) {
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
      return initialQuickOpenState();
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
