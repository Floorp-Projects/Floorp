/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { actions } = require("../constants");
const refresh = require("./refresh");

/**
 * Change the currently selected view.
 *
 * @param {viewState} view
 */
const changeView = exports.changeView = function (view) {
  return {
    type: actions.CHANGE_VIEW,
    view
  };
};

/**
 * Change the currently selected view and ensure all our data is up to date from
 * the heap worker.
 *
 * @param {viewState} view
 */
exports.changeViewAndRefresh = function (view, heapWorker) {
  return function* (dispatch, getState) {
    dispatch(changeView(view));
    yield dispatch(refresh.refresh(heapWorker));
  };
};
