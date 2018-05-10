"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.setPopupObjectProperties = setPopupObjectProperties;

var _selectors = require("../../selectors/index");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * @memberof actions/pause
 * @static
 */
function setPopupObjectProperties(object, properties) {
  return ({
    dispatch,
    client,
    getState
  }) => {
    const objectId = object.actor || object.objectId;

    if ((0, _selectors.getPopupObjectProperties)(getState(), object.actor)) {
      return;
    }

    dispatch({
      type: "SET_POPUP_OBJECT_PROPERTIES",
      objectId,
      properties
    });
  };
}