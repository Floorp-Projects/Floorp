/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cu } = require("chrome");
const Services = require("Services");

/**
 * Get the current theme from preferences.
 */
exports.getCurrentTheme = function () {
  return Services.prefs.getCharPref("devtools.theme");
};

/**
 * Export given object into the target window scope.
 */
exports.exportIntoContentScope = function (win, obj, defineAs) {
  let clone = Cu.createObjectIn(win, {
    defineAs: defineAs
  });

  let props = Object.getOwnPropertyNames(obj);
  for (let i = 0; i < props.length; i++) {
    let propName = props[i];
    let propValue = obj[propName];
    if (typeof propValue == "function") {
      Cu.exportFunction(propValue, clone, {
        defineAs: propName
      });
    }
  }
};
