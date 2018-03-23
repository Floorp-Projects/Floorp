/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  UPDATE_EDITOR_VISIBILITY,
} = require("../actions/index");

const INITIAL_STATE = {
  // Whether or not the font editor is visible.
  isVisible: false,
  // Selector text of the rule where font properties will be written.
  selector: "",
};

let reducers = {

  [UPDATE_EDITOR_VISIBILITY](state, { isVisible, selector }) {
    return { ...state, isVisible, selector };
  },

};

module.exports = function(state = INITIAL_STATE, action) {
  let reducer = reducers[action.type];
  if (!reducer) {
    return state;
  }
  return reducer(state, action);
};
