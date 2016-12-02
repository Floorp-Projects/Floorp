/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Colors for themes taken from:
 * https://developer.mozilla.org/en-US/docs/Tools/DevToolsColors
 */

const Services = require("Services");

const variableFileContents = require("raw!devtools/client/themes/variables.css");

const THEME_SELECTOR_STRINGS = {
  light: ":root.theme-light {",
  dark: ":root.theme-dark {",
  firebug: ":root.theme-firebug {"
};

/**
 * Takes a theme name and returns the contents of its variable rule block.
 * The first time this runs fetches the variables CSS file and caches it.
 */
function getThemeFile(name) {
  // If there's no theme expected for this name, use `light` as default.
  let selector = THEME_SELECTOR_STRINGS[name] ||
                 THEME_SELECTOR_STRINGS.light;

  // This is a pretty naive way to find the contents between:
  // selector {
  //   name: val;
  // }
  // There is test coverage for this feature (browser_theme.js)
  // so if an } is introduced in the variables file it will catch that.
  let theme = variableFileContents;
  theme = theme.substring(theme.indexOf(selector));
  theme = theme.substring(0, theme.indexOf("}"));

  return theme;
}

/**
 * Returns the string value of the current theme,
 * like "dark" or "light".
 */
const getTheme = exports.getTheme = () => {
  return Services.prefs.getCharPref("devtools.theme");
};

/**
 * Returns a color indicated by `type` (like "toolbar-background", or
 * "highlight-red"), with the ability to specify a theme, or use whatever the
 * current theme is if left unset. If theme not found, falls back to "light"
 * theme. Returns null if the type cannot be found for the theme given.
 */
/* eslint-disable no-unused-vars */
const getColor = exports.getColor = (type, theme) => {
  let themeName = theme || getTheme();
  let themeFile = getThemeFile(themeName);
  let match = themeFile.match(new RegExp("--theme-" + type + ": (.*);"));

  // Return the appropriate variable in the theme, or otherwise, null.
  return match ? match[1] : null;
};

/**
 * Set the theme preference.
 */
const setTheme = exports.setTheme = (newTheme) => {
  Services.prefs.setCharPref("devtools.theme", newTheme);
};
/* eslint-enable */
