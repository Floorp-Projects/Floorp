/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that theme utilities work

var {getColor, getTheme, setTheme} = require("devtools/client/shared/theme");

function test() {
  testGetTheme();
  testSetTheme();
  testGetColor();
  testColorExistence();
}

function testGetTheme () {
  let originalTheme = getTheme();
  ok(originalTheme, "has some theme to start with.");
  Services.prefs.setCharPref("devtools.theme", "light");
  is(getTheme(), "light", "getTheme() correctly returns light theme");
  Services.prefs.setCharPref("devtools.theme", "dark");
  is(getTheme(), "dark", "getTheme() correctly returns dark theme");
  Services.prefs.setCharPref("devtools.theme", "unknown");
  is(getTheme(), "unknown", "getTheme() correctly returns an unknown theme");
  Services.prefs.setCharPref("devtools.theme", originalTheme);
}

function testSetTheme () {
  let originalTheme = getTheme();
  gDevTools.once("pref-changed", (_, { pref, oldValue, newValue }) => {
    is(pref, "devtools.theme",
      "The 'pref-changed' event triggered by setTheme has correct pref.");
    is(oldValue, originalTheme,
      "The 'pref-changed' event triggered by setTheme has correct oldValue.");
    is(newValue, "dark",
      "The 'pref-changed' event triggered by setTheme has correct newValue.");
  });
  setTheme("dark");
  is(Services.prefs.getCharPref("devtools.theme"), "dark", "setTheme() correctly sets dark theme.");
  setTheme("light");
  is(Services.prefs.getCharPref("devtools.theme"), "light", "setTheme() correctly sets light theme.");
  setTheme("unknown");
  is(Services.prefs.getCharPref("devtools.theme"), "unknown", "setTheme() correctly sets an unknown theme.");
  Services.prefs.setCharPref("devtools.theme", originalTheme);
}

function testGetColor () {
  let BLUE_DARK = "#46afe3";
  let BLUE_LIGHT = "#0088cc";
  let originalTheme = getTheme();

  setTheme("dark");
  is(getColor("highlight-blue"), BLUE_DARK, "correctly gets color for enabled theme.");
  setTheme("light");
  is(getColor("highlight-blue"), BLUE_LIGHT, "correctly gets color for enabled theme.");
  setTheme("metal");
  is(getColor("highlight-blue"), BLUE_LIGHT, "correctly uses light for default theme if enabled theme not found");

  is(getColor("highlight-blue", "dark"), BLUE_DARK, "if provided and found, uses the provided theme.");
  is(getColor("highlight-blue", "metal"), BLUE_LIGHT, "if provided and not found, defaults to light theme.");
  is(getColor("somecomponents"), null, "if a type cannot be found, should return null.");

  setTheme(originalTheme);
}

function testColorExistence () {
  var vars = ["body-background", "sidebar-background", "contrast-background", "tab-toolbar-background",
   "toolbar-background", "selection-background", "selection-color",
   "selection-background-semitransparent", "splitter-color", "comment", "body-color",
   "body-color-alt", "content-color1", "content-color2", "content-color3",
   "highlight-green", "highlight-blue", "highlight-bluegrey", "highlight-purple",
   "highlight-lightorange", "highlight-orange", "highlight-red", "highlight-pink"
  ];

  for (let type of vars) {
    ok(getColor(type, "light"), `${type} is a valid color in light theme`);
    ok(getColor(type, "dark"), `${type} is a valid color in light theme`);
  }
}

function isColor (s) {
  // Regexes from Heather Arthur's `color-string`
  // https://github.com/harthur/color-string
  // MIT License
  return /^#([a-fA-F0-9]{3})$/.test(s) ||
         /^#([a-fA-F0-9]{6})$/.test(s) ||
         /^rgba?\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*(?:,\s*([\d\.]+)\s*)?\)$/.test(s) ||
         /^rgba?\(\s*([\d\.]+)\%\s*,\s*([\d\.]+)\%\s*,\s*([\d\.]+)\%\s*(?:,\s*([\d\.]+)\s*)?\)$/.test(s);
}
