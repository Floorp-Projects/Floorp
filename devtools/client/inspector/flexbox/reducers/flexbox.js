/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  CLEAR_FLEXBOX,
  UPDATE_FLEXBOX,
  UPDATE_FLEXBOX_COLOR,
  UPDATE_FLEXBOX_HIGHLIGHTED,
} = require("../actions/index");

const INITIAL_FLEXBOX = {
  // The color of the flexbox highlighter overlay.
  color: "",
  // The flex container of the selected element.
  flexContainer: {
    // The actor ID of the selected flex container.
    actorID: "",
    // An array of flex items belonging to the selected flex container.
    flexItems: [],
    // The NodeFront actor ID of the flex item to display in the flex item sizing
    // properties.
    flexItemShown: null,
    // This flag specifies that the flex container data represents the selected flex
    // container.
    isFlexItemContainer: false,
    // The NodeFront of the selected flex container.
    nodeFront: null,
    // The computed style properties of the selected flex container.
    properties: null,
  },
  // The selected flex container can also be a flex item. This object contains the
  // parent flex container properties of the selected element.
  flexItemContainer: {
    // The actor ID of the parent flex container.
    actorID: "",
    // An array of flex items belonging to the parent flex container.
    flexItems: [],
    // The NodeFront actor ID of the flex item to display in the flex item sizing
    // properties.
    flexItemShown: null,
    // This flag specifies that the flex container data represents the parent flex
    // container of the selected element.
    isFlexItemContainer: true,
    // The NodeFront of the parent flex container.
    nodeFront: null,
    // The computed styles properties of the parent flex container.
    properties: null,
  },
  // Whether or not the flexbox highlighter is highlighting the flex container.
  highlighted: false,
  // Whether or not the node selection that led to the flexbox tool being shown came from
  // the user selecting a node in the markup-view (whereas, say, selecting in the flex
  // items list)
  initiatedByMarkupViewSelection: false,
};

const reducers = {
  [CLEAR_FLEXBOX](flexbox, _) {
    return INITIAL_FLEXBOX;
  },

  [UPDATE_FLEXBOX](_, { flexbox }) {
    return flexbox;
  },

  [UPDATE_FLEXBOX_COLOR](flexbox, { color }) {
    return {
      ...flexbox,
      color,
    };
  },

  [UPDATE_FLEXBOX_HIGHLIGHTED](flexbox, { highlighted }) {
    return {
      ...flexbox,
      highlighted,
    };
  },
};

module.exports = function(flexbox = INITIAL_FLEXBOX, action) {
  const reducer = reducers[action.type];
  if (!reducer) {
    return flexbox;
  }
  return reducer(flexbox, action);
};
