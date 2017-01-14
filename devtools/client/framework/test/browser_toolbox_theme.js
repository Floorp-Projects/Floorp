/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const COMPACT_LIGHT_ID = "firefox-compact-light@mozilla.org";
const COMPACT_DARK_ID = "firefox-compact-dark@mozilla.org";
const PREF_DEVTOOLS_THEME = "devtools.theme";
const {LightweightThemeManager} = Components.utils.import("resource://gre/modules/LightweightThemeManager.jsm", {});

registerCleanupFunction(() => {
  // Set preferences back to their original values
  Services.prefs.clearUserPref(PREF_DEVTOOLS_THEME);
  LightweightThemeManager.currentTheme = null;
});

add_task(function* testDevtoolsTheme() {
  info("Checking stylesheet and :root attributes based on devtools theme.");
  Services.prefs.setCharPref(PREF_DEVTOOLS_THEME, "light");
  is(document.documentElement.getAttribute("devtoolstheme"), "light",
    "The documentElement has an attribute based on devtools theme.");

  Services.prefs.setCharPref(PREF_DEVTOOLS_THEME, "dark");
  is(document.documentElement.getAttribute("devtoolstheme"), "dark",
    "The documentElement has an attribute based on devtools theme.");

  Services.prefs.setCharPref(PREF_DEVTOOLS_THEME, "firebug");
  is(document.documentElement.getAttribute("devtoolstheme"), "light",
    "The documentElement has 'light' as a default for the devtoolstheme attribute");
});

add_task(function* testDevtoolsAndCompactThemeSyncing() {
  if (!AppConstants.INSTALL_COMPACT_THEMES) {
    ok(true, "No need to run this test since themes aren't installed");
    return;
  }

  info("Devtools theme light -> dark when dark compact applied");
  Services.prefs.setCharPref(PREF_DEVTOOLS_THEME, "light");
  LightweightThemeManager.currentTheme = LightweightThemeManager.getUsedTheme(COMPACT_DARK_ID);
  is(Services.prefs.getCharPref(PREF_DEVTOOLS_THEME), "dark");

  info("Devtools theme dark -> light when light compact applied");
  Services.prefs.setCharPref(PREF_DEVTOOLS_THEME, "dark");
  LightweightThemeManager.currentTheme = LightweightThemeManager.getUsedTheme(COMPACT_LIGHT_ID);
  is(Services.prefs.getCharPref(PREF_DEVTOOLS_THEME), "light");

  info("Devtools theme shouldn't change if it wasn't light or dark during lwt change");
  Services.prefs.setCharPref(PREF_DEVTOOLS_THEME, "firebug");
  LightweightThemeManager.currentTheme = LightweightThemeManager.getUsedTheme(COMPACT_DARK_ID);
  is(Services.prefs.getCharPref(PREF_DEVTOOLS_THEME), "firebug");

  info("Compact theme dark -> light when devtools changes dark -> light");
  LightweightThemeManager.currentTheme = LightweightThemeManager.getUsedTheme(COMPACT_DARK_ID);
  Services.prefs.setCharPref(PREF_DEVTOOLS_THEME, "dark");
  Services.prefs.setCharPref(PREF_DEVTOOLS_THEME, "light");
  is(LightweightThemeManager.currentTheme, LightweightThemeManager.getUsedTheme(COMPACT_LIGHT_ID));

  info("Compact theme dark -> light when devtools changes dark -> firebug");
  LightweightThemeManager.currentTheme = LightweightThemeManager.getUsedTheme(COMPACT_DARK_ID);
  Services.prefs.setCharPref(PREF_DEVTOOLS_THEME, "dark");
  Services.prefs.setCharPref(PREF_DEVTOOLS_THEME, "firebug");
  is(LightweightThemeManager.currentTheme, LightweightThemeManager.getUsedTheme(COMPACT_LIGHT_ID));

  info("Compact theme light -> dark when devtools changes light -> dark");
  LightweightThemeManager.currentTheme = LightweightThemeManager.getUsedTheme(COMPACT_LIGHT_ID);
  Services.prefs.setCharPref(PREF_DEVTOOLS_THEME, "light");
  Services.prefs.setCharPref(PREF_DEVTOOLS_THEME, "dark");
  is(LightweightThemeManager.currentTheme, LightweightThemeManager.getUsedTheme(COMPACT_DARK_ID));

  info("Compact theme shouldn't change if it wasn't set during devtools change");
  LightweightThemeManager.currentTheme = null;
  Services.prefs.setCharPref(PREF_DEVTOOLS_THEME, "light");
  Services.prefs.setCharPref(PREF_DEVTOOLS_THEME, "dark");
  is(LightweightThemeManager.currentTheme, null);
});
