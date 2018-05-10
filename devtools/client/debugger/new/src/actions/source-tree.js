"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.setExpandedState = setExpandedState;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function setExpandedState(expanded) {
  return ({
    dispatch,
    getState
  }) => {
    dispatch({
      type: "SET_EXPANDED_STATE",
      expanded
    });
  };
}