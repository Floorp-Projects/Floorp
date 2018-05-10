"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.setQuickOpenQuery = setQuickOpenQuery;
exports.openQuickOpen = openQuickOpen;
exports.closeQuickOpen = closeQuickOpen;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function setQuickOpenQuery(query) {
  return {
    type: "SET_QUICK_OPEN_QUERY",
    query
  };
}

function openQuickOpen(query) {
  if (query != null) {
    return {
      type: "OPEN_QUICK_OPEN",
      query
    };
  }

  return {
    type: "OPEN_QUICK_OPEN"
  };
}

function closeQuickOpen() {
  return {
    type: "CLOSE_QUICK_OPEN"
  };
}