/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { getStr } = require("../utils/l10n");

const {
  APPLY_FONT_VARIATION_INSTANCE,
  RESET_EDITOR,
  UPDATE_AXIS_VALUE,
  UPDATE_CUSTOM_INSTANCE,
  UPDATE_EDITOR_STATE,
  UPDATE_EDITOR_VISIBILITY,
} = require("../actions/index");

const INITIAL_STATE = {
  // Variable font axes.
  axes: {},
  // Copy of the most recent axes values. Used to revert from a named instance.
  customInstanceValues: [],
  // Fonts applicable to selected element.
  fonts: [],
  // Current selected font variation instance.
  instance: {
    name: getStr("fontinspector.customInstanceName"),
    values: [],
  },
  // Whether or not the font editor is visible.
  isVisible: false,
  // CSS font properties defined on the selected rule.
  properties: {},
  // Selector text of the selected rule where updated font properties will be written.
  selector: "",
};

let reducers = {

  // Update font editor with the axes and values defined by a font variation instance.
  [APPLY_FONT_VARIATION_INSTANCE](state, { name, values }) {
    let newState = { ...state };
    newState.instance.name = name;
    newState.instance.values = values;

    if (Array.isArray(values) && values.length) {
      newState.axes = values.reduce((acc, value) => {
        acc[value.axis] = value.value;
        return acc;
      }, {});
    }

    return newState;
  },

  [RESET_EDITOR](state) {
    return { ...INITIAL_STATE };
  },

  [UPDATE_AXIS_VALUE](state, { axis, value }) {
    let newState = { ...state };
    newState.axes[axis] = value;
    return newState;
  },

  // Copy state of current axes in the format of the "values" property of a named font
  // variation instance.
  [UPDATE_CUSTOM_INSTANCE](state) {
    const newState = { ...state };
    newState.customInstanceValues = Object.keys(state.axes).map(axis => {
      return { axis: [axis], value: state.axes[axis] };
    });
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
