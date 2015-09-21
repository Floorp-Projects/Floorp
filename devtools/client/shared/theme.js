/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Colors for themes taken from:
 * https://developer.mozilla.org/en-US/docs/Tools/DevToolsColors
 */

const { Ci, Cu } = require("chrome");
const { NetUtil } = Cu.import("resource://gre/modules/NetUtil.jsm", {});
loader.lazyRequireGetter(this, "Services");
loader.lazyImporter(this, "gDevTools", "resource:///modules/devtools/client/framework/gDevTools.jsm");

const themeURIs = {
  light: "chrome://browser/skin/devtools/light-theme.css",
  dark: "chrome://browser/skin/devtools/dark-theme.css"
}

const cachedThemes = {};

/**
 * Returns a string of the file found at URI
 */
function readURI (uri) {
  let stream = NetUtil.newChannel({
    uri: NetUtil.newURI(uri, "UTF-8"),
    loadUsingSystemPrincipal: true}
  ).open();

  let count = stream.available();
  let data = NetUtil.readInputStreamToString(stream, count, { charset: "UTF-8" });
  stream.close();
  return data;
}

/**
 * Takes a theme name and either returns it from the cache,
 * or fetches the theme CSS file and caches it.
 */
function getThemeFile (name) {
  // Use the cached theme, or generate it
  let themeFile = cachedThemes[name] || readURI(themeURIs[name]).match(/--theme-.*: .*;/g).join("\n");

  // Cache if not already cached
  if (!cachedThemes[name]) {
    cachedThemes[name] = themeFile;
  }

  return themeFile;
}

/**
 * Returns the string value of the current theme,
 * like "dark" or "light".
 */
const getTheme = exports.getTheme = () => Services.prefs.getCharPref("devtools.theme");

/**
 * Returns a color indicated by `type` (like "toolbar-background", or "highlight-red"),
 * with the ability to specify a theme, or use whatever the current theme is
 * if left unset. If theme not found, falls back to "light" theme. Returns null
 * if the type cannot be found for the theme given.
 */
const getColor = exports.getColor = (type, theme) => {
  let themeName = theme || getTheme();

  // If there's no theme URIs for this theme, use `light` as default.
  if (!themeURIs[themeName]) {
    themeName = "light";
  }

  let themeFile = getThemeFile(themeName);
  let match;

  // Return the appropriate variable in the theme, or otherwise, null.
  return (match = themeFile.match(new RegExp("--theme-" + type + ": (.*);"))) ? match[1] : null;
};

/**
 * Mimics selecting the theme selector in the toolbox;
 * sets the preference and emits an event on gDevTools to trigger
 * the themeing.
 */
const setTheme = exports.setTheme = (newTheme) => {
  let oldTheme = getTheme();

  Services.prefs.setCharPref("devtools.theme", newTheme);
  gDevTools.emit("pref-changed", {
    pref: "devtools.theme",
    newValue: newTheme,
    oldValue: oldTheme
  });
};
