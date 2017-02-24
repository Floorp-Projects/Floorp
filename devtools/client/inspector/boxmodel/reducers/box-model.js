/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  UPDATE_LAYOUT,
} = require("../actions/index");

const INITIAL_BOX_MODEL = {
  layout: {},
};

let reducers = {

  [UPDATE_LAYOUT](boxModel, { layout }) {
    return Object.assign({}, boxModel, {
      layout,
    });
  },

};

module.exports = function (boxModel = INITIAL_BOX_MODEL, action) {
  let reducer = reducers[action.type];
  if (!reducer) {
    return boxModel;
  }
  return reducer(boxModel, action);
};
