/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");

const {
  TOGGLE_LEFT_ALIGNMENT,
} = require("../actions/index");

const LEFT_ALIGNMENT_ENABLED = "devtools.responsive.leftAlignViewport.enabled";

const INITIAL_UI = {
  // Whether or not the viewports are left aligned.
  leftAlignmentEnabled: Services.prefs.getBoolPref(LEFT_ALIGNMENT_ENABLED),
};

const reducers = {

  [TOGGLE_LEFT_ALIGNMENT](ui, { enabled }) {
    const leftAlignmentEnabled = enabled !== undefined ?
      enabled : !ui.leftAlignmentEnabled;

    Services.prefs.setBoolPref(LEFT_ALIGNMENT_ENABLED, leftAlignmentEnabled);

    return Object.assign({}, ui, {
      leftAlignmentEnabled,
    });
  },

};

module.exports = function(ui = INITIAL_UI, action) {
  const reducer = reducers[action.type];
  if (!reducer) {
    return ui;
  }
  return reducer(ui, action);
};
