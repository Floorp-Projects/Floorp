/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  UPDATE_ANIMATIONS,
  UPDATE_DETAIL_VISIBILITY,
  UPDATE_ELEMENT_PICKER_ENABLED,
  UPDATE_SELECTED_ANIMATION,
  UPDATE_SIDEBAR_SIZE,
} = require("../actions/index");

const INITIAL_STATE = {
  animations: [],
  detailVisibility: false,
  elementPickerEnabled: false,
  selectedAnimation: null,
  sidebarSize: {
    height: 0,
    width: 0,
  },
};

const reducers = {
  [UPDATE_ANIMATIONS](state, { animations }) {
    return Object.assign({}, state, {
      animations,
    });
  },

  [UPDATE_DETAIL_VISIBILITY](state, { detailVisibility }) {
    return Object.assign({}, state, {
      detailVisibility
    });
  },

  [UPDATE_ELEMENT_PICKER_ENABLED](state, { elementPickerEnabled }) {
    return Object.assign({}, state, {
      elementPickerEnabled
    });
  },

  [UPDATE_SELECTED_ANIMATION](state, { selectedAnimation }) {
    const detailVisibility = !!selectedAnimation;

    return Object.assign({}, state, {
      detailVisibility,
      selectedAnimation
    });
  },

  [UPDATE_SIDEBAR_SIZE](state, { sidebarSize }) {
    return Object.assign({}, state, {
      sidebarSize
    });
  },
};

module.exports = function (state = INITIAL_STATE, action) {
  const reducer = reducers[action.type];
  return reducer ? reducer(state, action) : state;
};
