/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  RESET_EDITOR,
  UPDATE_AXIS_VALUE,
  UPDATE_EDITOR_STATE,
  UPDATE_EDITOR_VISIBILITY,
} = require("../actions/index");

const INITIAL_STATE = {
  // Variable font axes.
  axes: {},
  // Fonts applicable to selected element.
  fonts: [],
  // Whether or not the font editor is visible.
  isVisible: false,
  // CSS font properties defined on the selected rule.
  properties: {},
  // Selector text of the selected rule where updated font properties will be written.
  selector: "",
};

let reducers = {

  [RESET_EDITOR](state) {
    return { ...INITIAL_STATE };
  },

  [UPDATE_AXIS_VALUE](state, { axis, value }) {
    let newState = { ...state };
    newState.axes[axis] = value;
    return newState;
  },

  [UPDATE_EDITOR_STATE](state, { fonts, properties }) {
    let axes = {};

    if (properties["font-variation-settings"]) {
      // Parse font-variation-settings CSS declaration into an object
      // with axis tags as keys and axis values as values.
      axes = properties["font-variation-settings"]
        .split(",")
        .reduce((acc, pair) => {
          // Tags are always in quotes. Split by quote and filter excessive whitespace.
          pair = pair.split(/["']/).filter(part => part.trim() !== "");
          const tag = pair[0].trim();
          const value = pair[1].trim();
          acc[tag] = value;
          return acc;
        }, {});
    }

    return { ...state, axes, fonts, properties };
  },

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
