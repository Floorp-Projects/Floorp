"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.selectComponent = selectComponent;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function selectComponent(componentIndex) {
  return async ({
    dispatch
  }) => {
    dispatch({
      type: "SELECT_COMPONENT",
      componentIndex
    });
  };
}