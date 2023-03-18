/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * File Search reducer
 * @module reducers/fileSearch
 */

const emptySearchResults = Object.freeze({
  matches: Object.freeze([]),
  matchIndex: -1,
  index: -1,
  count: 0,
});

export const initialFileSearchState = () => ({
  query: "",
  searchResults: emptySearchResults,
});

function update(state = initialFileSearchState(), action) {
  switch (action.type) {
    case "UPDATE_FILE_SEARCH_QUERY": {
      return { ...state, query: action.query };
    }

    case "UPDATE_SEARCH_RESULTS": {
      return { ...state, searchResults: action.results };
    }

    case "NAVIGATE": {
      return { ...state, query: "", searchResults: emptySearchResults };
    }

    default: {
      return state;
    }
  }
}

export default update;
