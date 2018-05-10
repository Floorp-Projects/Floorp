"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.createQuickOpenState = undefined;
exports.default = update;
exports.getQuickOpenEnabled = getQuickOpenEnabled;
exports.getQuickOpenQuery = getQuickOpenQuery;
exports.getQuickOpenType = getQuickOpenType;

var _makeRecord = require("../utils/makeRecord");

var _makeRecord2 = _interopRequireDefault(_makeRecord);

var _quickOpen = require("../utils/quick-open");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Quick Open reducer
 * @module reducers/quick-open
 */
const createQuickOpenState = exports.createQuickOpenState = (0, _makeRecord2.default)({
  enabled: false,
  query: "",
  searchType: "sources"
});

function update(state = createQuickOpenState(), action) {
  switch (action.type) {
    case "OPEN_QUICK_OPEN":
      if (action.query != null) {
        return state.merge({
          enabled: true,
          query: action.query,
          searchType: (0, _quickOpen.parseQuickOpenQuery)(action.query)
        });
      }

      return state.set("enabled", true);

    case "CLOSE_QUICK_OPEN":
      return createQuickOpenState();

    case "SET_QUICK_OPEN_QUERY":
      return state.merge({
        query: action.query,
        searchType: (0, _quickOpen.parseQuickOpenQuery)(action.query)
      });

    default:
      return state;
  }
}

function getQuickOpenEnabled(state) {
  return state.quickOpen.get("enabled");
}

function getQuickOpenQuery(state) {
  return state.quickOpen.get("query");
}

function getQuickOpenType(state) {
  return state.quickOpen.get("searchType");
}