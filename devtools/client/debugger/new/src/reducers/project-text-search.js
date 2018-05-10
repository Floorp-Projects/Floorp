"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.statusType = undefined;
exports.initialProjectTextSearchState = initialProjectTextSearchState;
exports.getTextSearchResults = getTextSearchResults;
exports.getTextSearchStatus = getTextSearchStatus;
exports.getTextSearchQuery = getTextSearchQuery;

var _immutable = require("devtools/client/shared/vendor/immutable");

var I = _interopRequireWildcard(_immutable);

var _makeRecord = require("../utils/makeRecord");

var _makeRecord2 = _interopRequireDefault(_makeRecord);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
// @format

/**
 * Project text search reducer
 * @module reducers/project-text-search
 */
const statusType = exports.statusType = {
  initial: "INITIAL",
  fetching: "FETCHING",
  done: "DONE",
  error: "ERROR"
};

function initialProjectTextSearchState() {
  return (0, _makeRecord2.default)({
    query: "",
    results: I.List(),
    status: statusType.initial
  })();
}

function update(state = initialProjectTextSearchState(), action) {
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
      return state.merge({
        results: results.push(action.result)
      });

    case "UPDATE_STATUS":
      return state.merge({
        status: action.status
      });

    case "CLEAR_SEARCH_RESULTS":
      return state.merge({
        results: state.get("results").clear()
      });

    case "CLEAR_SEARCH":
    case "CLOSE_PROJECT_SEARCH":
      return state.merge({
        query: "",
        results: state.get("results").clear(),
        status: statusType.initial
      });
  }

  return state;
}

function getTextSearchResults(state) {
  return state.projectTextSearch.get("results");
}

function getTextSearchStatus(state) {
  return state.projectTextSearch.get("status");
}

function getTextSearchQuery(state) {
  return state.projectTextSearch.get("query");
}

exports.default = update;