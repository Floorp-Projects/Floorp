/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  DEFAULT_FILTERS_VALUES,
  FILTERS,
  PREFS
} = require("devtools/client/webconsole/new-console-output/constants");

function getDefaultPrefs() {
  return Object.assign({
    "devtools.hud.loglimit": 1000,
    [PREFS.UI.FILTER_BAR]: false,
    [PREFS.UI.PERSIST]: false,
  }, Object.entries(PREFS.FILTER).reduce((res, [key, pref]) => {
    res[pref] = DEFAULT_FILTERS_VALUES[FILTERS[key]];
    return res;
  }, {}));
}

let prefs = Object.assign({}, getDefaultPrefs());

module.exports = {
  prefs: {
    getIntPref: pref => prefs[pref],
    getBoolPref: pref => prefs[pref],
    setBoolPref: (pref, value) => {
      prefs[pref] = value;
    },
    clearUserPref: (pref) => {
      prefs[pref] = (getDefaultPrefs())[pref];
    },
    testHelpers: {
      getAllPrefs: () => prefs,
      getFiltersPrefs: () => Object.values(PREFS.FILTER).reduce((res, pref) => {
        res[pref] = prefs[pref];
        return res;
      }, {}),
      clearPrefs: () => {
        prefs = Object.assign({}, getDefaultPrefs());
      }
    }
  }
};
