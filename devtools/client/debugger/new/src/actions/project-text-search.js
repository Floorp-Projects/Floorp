"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.addSearchQuery = addSearchQuery;
exports.clearSearchQuery = clearSearchQuery;
exports.clearSearchResults = clearSearchResults;
exports.clearSearch = clearSearch;
exports.updateSearchStatus = updateSearchStatus;
exports.closeProjectSearch = closeProjectSearch;
exports.searchSources = searchSources;
exports.searchSource = searchSource;

var _search = require("../workers/search/index");

var _selectors = require("../selectors/index");

var _source = require("../utils/source");

var _loadSourceText = require("./sources/loadSourceText");

var _projectTextSearch = require("../reducers/project-text-search");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Redux actions for the search state
 * @module actions/search
 */
function addSearchQuery(query) {
  return {
    type: "ADD_QUERY",
    query
  };
}

function clearSearchQuery() {
  return {
    type: "CLEAR_QUERY"
  };
}

function clearSearchResults() {
  return {
    type: "CLEAR_SEARCH_RESULTS"
  };
}

function clearSearch() {
  return {
    type: "CLEAR_SEARCH"
  };
}

function updateSearchStatus(status) {
  return {
    type: "UPDATE_STATUS",
    status
  };
}

function closeProjectSearch() {
  return {
    type: "CLOSE_PROJECT_SEARCH"
  };
}

function searchSources(query) {
  return async ({
    dispatch,
    getState
  }) => {
    await dispatch(clearSearchResults());
    await dispatch(addSearchQuery(query));
    dispatch(updateSearchStatus(_projectTextSearch.statusType.fetching));
    const sources = (0, _selectors.getSources)(getState());
    const validSources = sources.valueSeq().filter(source => !(0, _selectors.hasPrettySource)(getState(), source.id) && !(0, _source.isThirdParty)(source));

    for (const source of validSources) {
      await dispatch((0, _loadSourceText.loadSourceText)(source));
      await dispatch(searchSource(source.id, query));
    }

    dispatch(updateSearchStatus(_projectTextSearch.statusType.done));
  };
}

function searchSource(sourceId, query) {
  return async ({
    dispatch,
    getState
  }) => {
    const sourceRecord = (0, _selectors.getSource)(getState(), sourceId);

    if (!sourceRecord) {
      return;
    }

    const matches = await (0, _search.findSourceMatches)(sourceRecord.toJS(), query);

    if (!matches.length) {
      return;
    }

    dispatch({
      type: "ADD_SEARCH_RESULT",
      result: {
        sourceId: sourceRecord.id,
        filepath: sourceRecord.url,
        matches
      }
    });
  };
}