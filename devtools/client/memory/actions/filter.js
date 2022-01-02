/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { actions } = require("devtools/client/memory/constants");
const { refresh } = require("devtools/client/memory/actions/refresh");
const { debounce } = require("devtools/shared/debounce");

const setFilterString = (exports.setFilterString = function(filterString) {
  return {
    type: actions.SET_FILTER_STRING,
    filter: filterString,
  };
});

// The number of milliseconds we should wait before kicking off a new census
// when the filter string is updated. This helps us avoid doing any work while
// the user is still typing.
const FILTER_INPUT_DEBOUNCE_MS = 250;
const debouncedRefreshDispatcher = debounce(
  (dispatch, heapWorker) => dispatch(refresh(heapWorker)),
  FILTER_INPUT_DEBOUNCE_MS
);

exports.setFilterStringAndRefresh = function(filterString, heapWorker) {
  return ({ dispatch, getState }) => {
    dispatch(setFilterString(filterString));
    debouncedRefreshDispatcher(dispatch, heapWorker);
  };
};
