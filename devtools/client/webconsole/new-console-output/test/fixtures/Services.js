/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

module.exports = {
  prefs: {
    getIntPref: pref => {
      switch (pref) {
        case "devtools.hud.loglimit":
          return 1000;
      }
    },
    getBoolPref: pref => {
      switch (pref) {
        default:
          return true;
      }
    }
  }
};
