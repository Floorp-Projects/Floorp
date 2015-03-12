/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Tool } = require("dev/toolbox");
const { Class } = require("sdk/core/heritage");
const { onEnable, onDisable } = require("dev/theme/hooks");
const { Theme, LightTheme } = require("dev/theme");

/**
 * This object represents a new theme registered within the Toolbox.
 * You can activate it by clicking on "My Light Theme" theme option
 * in the Options panel.
 * Note that the new theme derives styles from built-in Light theme.
 */
const MyTheme = Theme({
  name: "mytheme",
  label: "My Light Theme",
  styles: [LightTheme, "./theme.css"],

  onEnable: function(window, oldTheme) {
    console.log("myTheme.onEnable; method override " +
      window.location.href);
  },
  onDisable: function(window, newTheme) {
    console.log("myTheme.onDisable; method override " +
      window.location.href);
  },
});

// Registration

const mytheme = new Tool({
  name: "My Tool",
  themes: { mytheme: MyTheme }
});
