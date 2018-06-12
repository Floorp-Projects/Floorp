/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");

const {
  UPDATE_SHOW_GRID_AREAS,
  UPDATE_SHOW_GRID_LINE_NUMBERS,
  UPDATE_SHOW_INFINITE_LINES
} = require("../actions/index");

const SHOW_GRID_AREAS = "devtools.gridinspector.showGridAreas";
const SHOW_GRID_LINE_NUMBERS = "devtools.gridinspector.showGridLineNumbers";
const SHOW_INFINITE_LINES = "devtools.gridinspector.showInfiniteLines";

const INITIAL_HIGHLIGHTER_SETTINGS = () => {
  return {
    showGridAreasOverlay: Services.prefs.getBoolPref(SHOW_GRID_AREAS),
    showGridLineNumbers: Services.prefs.getBoolPref(SHOW_GRID_LINE_NUMBERS),
    showInfiniteLines: Services.prefs.getBoolPref(SHOW_INFINITE_LINES),
  };
};

const reducers = {

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

module.exports = function(highlighterSettings = INITIAL_HIGHLIGHTER_SETTINGS(), action) {
  const reducer = reducers[action.type];
  if (!reducer) {
    return highlighterSettings;
  }
  return reducer(highlighterSettings, action);
};
