/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function getPreferenceName(hud, suffix) {
  if (!suffix) {
    console.error("Suffix shouldn't be falsy", { suffix });
    return null;
  }

  if (!hud) {
    console.error("hud shouldn't be falsy", { hud });
    return null;
  }

  if (suffix.startsWith("devtools.")) {
    // We don't have a suffix but a full pref name. Let's return it.
    return suffix;
  }

  const component = hud.isBrowserConsole ? "browserconsole" : "webconsole";
  return `devtools.${component}.${suffix}`;
}

function getPrefsService(hud) {
  const getPrefName = pref => getPreferenceName(hud, pref);

  return {
    getBoolPref: (pref, deflt) =>
      Services.prefs.getBoolPref(getPrefName(pref), deflt),
    getIntPref: (pref, deflt) =>
      Services.prefs.getIntPref(getPrefName(pref), deflt),
    setBoolPref: (pref, value) =>
      Services.prefs.setBoolPref(getPrefName(pref), value),
    setIntPref: (pref, value) =>
      Services.prefs.setIntPref(getPrefName(pref), value),
    clearUserPref: pref => Services.prefs.clearUserPref(getPrefName(pref)),
    getPrefName,
  };
}

module.exports = {
  getPrefsService,
};
