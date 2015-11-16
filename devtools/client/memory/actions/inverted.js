/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { actions } = require("../constants");
const { refresh } = require("./refresh");

const toggleInverted = exports.toggleInverted = function () {
  return { type: actions.TOGGLE_INVERTED };
};

exports.toggleInvertedAndRefresh = function (heapWorker) {
  return function* (dispatch, getState) {
    dispatch(toggleInverted());
    yield dispatch(refresh(heapWorker));
  };
};
