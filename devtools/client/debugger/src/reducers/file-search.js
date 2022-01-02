/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * File Search reducer
 * @module reducers/fileSearch
 */

import { prefs } from "../utils/prefs";

const emptySearchResults = Object.freeze({
  matches: Object.freeze([]),
  matchIndex: -1,
  index: -1,
  count: 0,
});

export const initialFileSearchState = () => ({
  query: "",
  searchResults: emptySearchResults,
  modifiers: {
    caseSensitive: prefs.fileSearchCaseSensitive,
    wholeWord: prefs.fileSearchWholeWord,
    regexMatch: prefs.fileSearchRegexMatch,
  },
});

function update(state = initialFileSearchState(), action) {
  switch (action.type) {
    case "UPDATE_FILE_SEARCH_QUERY": {
      return { ...state, query: action.query };
    }

    case "UPDATE_SEARCH_RESULTS": {
      return { ...state, searchResults: action.results };
    }

    case "TOGGLE_FILE_SEARCH_MODIFIER": {
      const actionVal = !state.modifiers[action.modifier];

      if (action.modifier == "caseSensitive") {
        prefs.fileSearchCaseSensitive = actionVal;
      }

      if (action.modifier == "wholeWord") {
        prefs.fileSearchWholeWord = actionVal;
      }

      if (action.modifier == "regexMatch") {
        prefs.fileSearchRegexMatch = actionVal;
      }

      return {
        ...state,
        modifiers: { ...state.modifiers, [action.modifier]: actionVal },
      };
    }

    case "NAVIGATE": {
      return { ...state, query: "", searchResults: emptySearchResults };
    }

    default: {
      return state;
    }
  }
}

// NOTE: we'd like to have the app state fully typed
// https://github.com/firefox-devtools/debugger/blob/master/src/reducers/sources.js#L179-L185

export function getFileSearchQuery(state) {
  return state.fileSearch.query;
}

export function getFileSearchModifiers(state) {
  return state.fileSearch.modifiers;
}

export function getFileSearchResults(state) {
  return state.fileSearch.searchResults;
}

export default update;
