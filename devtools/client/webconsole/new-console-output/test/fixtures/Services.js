/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { PREFS } = require("devtools/client/webconsole/new-console-output/constants");

module.exports = {
  prefs: {
    getIntPref: pref => {
      switch (pref) {
        case "devtools.hud.loglimit":
          return 1000;
      }
    },
    getBoolPref: pref => {
      const falsey = [
        PREFS.FILTER.NET,
        PREFS.FILTER.NETXHR,
        PREFS.UI.FILTER_BAR,
      ];
      return !falsey.includes(pref);
    },
    setBoolPref: () => {},
    clearUserPref: () => {},
  }
};
