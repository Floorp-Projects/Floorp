/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  CHANGE_RELOAD_CONDITION,
  LOAD_RELOAD_CONDITIONS_END,
} = require("./index");

const Types = require("../types");
const Services = require("Services");

const PREF_PREFIX = "devtools.responsive.reloadConditions.";

module.exports = {

  changeReloadCondition(id, value) {
    return dispatch => {
      const pref = PREF_PREFIX + id;
      Services.prefs.setBoolPref(pref, value);
      dispatch({
        type: CHANGE_RELOAD_CONDITION,
        id,
        value,
      });
    };
  },

  loadReloadConditions() {
    return dispatch => {
      // Loop over the conditions and load their values from prefs.
      for (const id in Types.reloadConditions) {
        // Skip over the loading state of the list.
        if (id == "state") {
          return;
        }
        const pref = PREF_PREFIX + id;
        const value = Services.prefs.getBoolPref(pref, false);
        dispatch({
          type: CHANGE_RELOAD_CONDITION,
          id,
          value,
        });
      }

      dispatch({ type: LOAD_RELOAD_CONDITIONS_END });
    };
  },

};
