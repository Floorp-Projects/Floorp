/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  UPDATE_ANIMATIONS,
  UPDATE_DETAIL_VISIBILITY,
  UPDATE_ELEMENT_PICKER_ENABLED,
  UPDATE_HIGHLIGHTED_NODE,
  UPDATE_SELECTED_ANIMATION,
  UPDATE_SIDEBAR_SIZE,
} = require("../actions/index");

const TimeScale = require("../utils/timescale");

const INITIAL_STATE = {
  animations: [],
  detailVisibility: false,
  elementPickerEnabled: false,
  highlightedNode: null,
  selectedAnimation: null,
  sidebarSize: {
    height: 0,
    width: 0,
  },
  timeScale: null,
};

const reducers = {
  [UPDATE_ANIMATIONS](state, { animations }) {
    let detailVisibility = state.detailVisibility;
    let selectedAnimation = state.selectedAnimation;

    if (!state.selectedAnimation ||
        !animations.find(animation => animation.actorID === selectedAnimation.actorID)) {
      selectedAnimation = animations.length === 1 ? animations[0] : null;
      detailVisibility = !!selectedAnimation;
    }

    return Object.assign({}, state, {
      animations,
      detailVisibility,
      selectedAnimation,
      timeScale: new TimeScale(animations),
    });
  },

  [UPDATE_DETAIL_VISIBILITY](state, { detailVisibility }) {
    const selectedAnimation =
      detailVisibility ? state.selectedAnimation : null;

    return Object.assign({}, state, {
      detailVisibility,
      selectedAnimation,
    });
  },

  [UPDATE_ELEMENT_PICKER_ENABLED](state, { elementPickerEnabled }) {
    return Object.assign({}, state, {
      elementPickerEnabled
    });
  },

  [UPDATE_HIGHLIGHTED_NODE](state, { highlightedNode }) {
    return Object.assign({}, state, {
      highlightedNode
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

module.exports = function(state = INITIAL_STATE, action) {
  const reducer = reducers[action.type];
  return reducer ? reducer(state, action) : state;
};
