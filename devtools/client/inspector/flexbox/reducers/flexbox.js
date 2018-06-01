/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  CLEAR_FLEXBOX,
  UPDATE_FLEXBOX,
  UPDATE_FLEXBOX_HIGHLIGHTED,
} = require("../actions/index");

const INITIAL_FLEXBOX = {
  // The actor ID of the flex container.
  actorID: null,
  // Whether or not the flexbox highlighter is highlighting the flex container.
  highlighted: false,
  // The NodeFront of the flex container.
  nodeFront: null,
};

const reducers = {

  [CLEAR_FLEXBOX](flexbox, _) {
    return INITIAL_FLEXBOX;
  },

  [UPDATE_FLEXBOX](_, { flexbox }) {
    return flexbox;
  },

  [UPDATE_FLEXBOX_HIGHLIGHTED](flexbox, { highlighted }) {
    return Object.assign({}, flexbox, {
      highlighted,
    });
  },

};

module.exports = function(flexbox = INITIAL_FLEXBOX, action) {
  const reducer = reducers[action.type];
  if (!reducer) {
    return flexbox;
  }
  return reducer(flexbox, action);
};
