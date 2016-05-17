/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { assert } = require("devtools/shared/DevToolsUtils");
const { actions } = require("../constants");
const { findSelectedSnapshot } = require("../utils");
const refresh = require("./refresh");

/**
 * Change the currently selected view.
 *
 * @param {viewState} view
 */
const changeView = exports.changeView = function (view) {
  return function (dispatch, getState) {
    dispatch({
      type: actions.CHANGE_VIEW,
      newViewState: view,
      oldDiffing: getState().diffing,
      oldSelected: findSelectedSnapshot(getState()),
    });
  };
};

/**
 * Given that we are in the INDIVIDUALS view state, go back to the state we were
 * in before.
 */
const popView = exports.popView = function () {
  return function (dispatch, getState) {
    const { previous } = getState().view;
    assert(previous);
    dispatch({
      type: actions.POP_VIEW,
      previousView: previous,
    });
  };
};

/**
 * Change the currently selected view and ensure all our data is up to date from
 * the heap worker.
 *
 * @param {viewState} view
 * @param {HeapAnalysesClient} heapWorker
 */
exports.changeViewAndRefresh = function (view, heapWorker) {
  return function* (dispatch, getState) {
    dispatch(changeView(view));
    yield dispatch(refresh.refresh(heapWorker));
  };
};

/**
 * Given that we are in the INDIVIDUALS view state, go back to the state we were
 * previously in and refresh our data.
 *
 * @param {HeapAnalysesClient} heapWorker
 */
exports.popViewAndRefresh = function (heapWorker) {
  return function* (dispatch, getState) {
    dispatch(popView());
    yield dispatch(refresh.refresh(heapWorker));
  };
};
