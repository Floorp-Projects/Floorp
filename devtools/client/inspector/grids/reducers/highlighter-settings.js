/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  UPDATE_SHOW_GRID_AREAS,
  UPDATE_SHOW_GRID_LINE_NUMBERS,
  UPDATE_SHOW_INFINITE_LINES
} = require("../actions/index");

const INITIAL_HIGHLIGHTER_SETTINGS = {
  showGridAreasOverlay: false,
  showGridLineNumbers: false,
  showInfiniteLines: false,
};

let reducers = {

  [UPDATE_SHOW_GRID_AREAS](highlighterSettings, { enabled }) {
    return Object.assign({}, highlighterSettings, {
      showGridAreasOverlay: enabled,
    });
  },

  [UPDATE_SHOW_GRID_LINE_NUMBERS](highlighterSettings, { enabled }) {
    return Object.assign({}, highlighterSettings, {
      showGridLineNumbers: enabled,
    });
  },

  [UPDATE_SHOW_INFINITE_LINES](highlighterSettings, { enabled }) {
    return Object.assign({}, highlighterSettings, {
      showInfiniteLines: enabled,
    });
  },

};

module.exports = function (highlighterSettings = INITIAL_HIGHLIGHTER_SETTINGS, action) {
  let reducer = reducers[action.type];
  if (!reducer) {
    return highlighterSettings;
  }
  return reducer(highlighterSettings, action);
};
