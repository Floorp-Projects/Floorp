/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  UPDATE_ANIMATIONS,
  UPDATE_DETAIL_VISIBILITY,
  UPDATE_ELEMENT_PICKER_ENABLED,
  UPDATE_HIGHLIGHTED_NODE,
  UPDATE_PLAYBACK_RATES,
  UPDATE_SELECTED_ANIMATION,
  UPDATE_SIDEBAR_SIZE,
} = require("devtools/client/inspector/animation/actions/index");

loader.lazyRequireGetter(
  this,
  "TimeScale",
  "devtools/client/inspector/animation/utils/timescale"
);

const INITIAL_STATE = {
  animations: [],
  detailVisibility: false,
  elementPickerEnabled: false,
  highlightedNode: null,
  playbackRates: [],
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

    if (
      !state.selectedAnimation ||
      !animations.find(
        animation => animation.actorID === selectedAnimation.actorID
      )
    ) {
      selectedAnimation = animations.length === 1 ? animations[0] : null;
      detailVisibility = !!selectedAnimation;
    }

    const playbackRates = getPlaybackRates(state.playbackRates, animations);

    return Object.assign({}, state, {
      animations,
      detailVisibility,
      playbackRates,
      selectedAnimation,
      timeScale: new TimeScale(animations),
    });
  },

  [UPDATE_DETAIL_VISIBILITY](state, { detailVisibility }) {
    const selectedAnimation = detailVisibility ? state.selectedAnimation : null;

    return Object.assign({}, state, {
      detailVisibility,
      selectedAnimation,
    });
  },

  [UPDATE_ELEMENT_PICKER_ENABLED](state, { elementPickerEnabled }) {
    return Object.assign({}, state, {
      elementPickerEnabled,
    });
  },

  [UPDATE_HIGHLIGHTED_NODE](state, { highlightedNode }) {
    return Object.assign({}, state, {
      highlightedNode,
    });
  },

  [UPDATE_PLAYBACK_RATES](state) {
    return Object.assign({}, state, {
      playbackRates: getPlaybackRates([], state.animations),
    });
  },

  [UPDATE_SELECTED_ANIMATION](state, { selectedAnimation }) {
    const detailVisibility = !!selectedAnimation;

    return Object.assign({}, state, {
      detailVisibility,
      selectedAnimation,
    });
  },

  [UPDATE_SIDEBAR_SIZE](state, { sidebarSize }) {
    return Object.assign({}, state, {
      sidebarSize,
    });
  },
};

function getPlaybackRates(basePlaybackRate, animations) {
  return [
    ...new Set(
      animations.map(a => a.state.playbackRate).concat(basePlaybackRate)
    ),
  ];
}

module.exports = function(state = INITIAL_STATE, action) {
  const reducer = reducers[action.type];
  return reducer ? reducer(state, action) : state;
};
