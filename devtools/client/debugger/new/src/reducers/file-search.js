"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.createFileSearchState = undefined;
exports.getFileSearchQuery = getFileSearchQuery;
exports.getFileSearchModifiers = getFileSearchModifiers;
exports.getFileSearchResults = getFileSearchResults;

var _makeRecord = require("../utils/makeRecord");

var _makeRecord2 = _interopRequireDefault(_makeRecord);

var _prefs = require("../utils/prefs");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * File Search reducer
 * @module reducers/fileSearch
 */
const createFileSearchState = exports.createFileSearchState = (0, _makeRecord2.default)({
  query: "",
  searchResults: {
    matches: [],
    matchIndex: -1,
    index: -1,
    count: 0
  },
  modifiers: (0, _makeRecord2.default)({
    caseSensitive: _prefs.prefs.fileSearchCaseSensitive,
    wholeWord: _prefs.prefs.fileSearchWholeWord,
    regexMatch: _prefs.prefs.fileSearchRegexMatch
  })()
});

function update(state = createFileSearchState(), action) {
  switch (action.type) {
    case "UPDATE_FILE_SEARCH_QUERY":
      {
        return state.set("query", action.query);
      }

    case "UPDATE_SEARCH_RESULTS":
      {
        return state.set("searchResults", action.results);
      }

    case "TOGGLE_FILE_SEARCH_MODIFIER":
      {
        const actionVal = !state.modifiers[action.modifier];

        if (action.modifier == "caseSensitive") {
          _prefs.prefs.fileSearchCaseSensitive = actionVal;
        }

        if (action.modifier == "wholeWord") {
          _prefs.prefs.fileSearchWholeWord = actionVal;
        }

        if (action.modifier == "regexMatch") {
          _prefs.prefs.fileSearchRegexMatch = actionVal;
        }

        return state.setIn(["modifiers", action.modifier], actionVal);
      }

    default:
      {
        return state;
      }
  }
} // NOTE: we'd like to have the app state fully typed
// https://github.com/devtools-html/debugger.html/blob/master/src/reducers/sources.js#L179-L185


function getFileSearchQuery(state) {
  return state.fileSearch.query;
}

function getFileSearchModifiers(state) {
  return state.fileSearch.modifiers;
}

function getFileSearchResults(state) {
  return state.fileSearch.searchResults;
}

exports.default = update;