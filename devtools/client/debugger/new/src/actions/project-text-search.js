"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.addSearchQuery = addSearchQuery;
exports.clearSearchQuery = clearSearchQuery;
exports.addSearchResult = addSearchResult;
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

function addSearchResult(sourceId, filepath, matches) {
  return {
    type: "ADD_SEARCH_RESULT",
    result: {
      sourceId,
      filepath,
      matches
    }
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
    const validSources = (0, _selectors.getSourceList)(getState()).filter(source => !(0, _selectors.hasPrettySource)(getState(), source.id) && !(0, _source.isThirdParty)(source));

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
    const source = (0, _selectors.getSource)(getState(), sourceId);

    if (!source) {
      return;
    }

    const matches = await (0, _search.findSourceMatches)(source, query);

    if (!matches.length) {
      return;
    }

    dispatch(addSearchResult(source.id, source.url, matches));
  };
}