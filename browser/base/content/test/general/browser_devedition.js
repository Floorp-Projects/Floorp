/*
 * Testing changes for Developer Edition theme.
 * A special stylesheet should be added to the browser.xul document
 * when browser.devedition.theme.enabled is set to true and no themes
 * are applied.
 */

const PREF_DEVEDITION_THEME = "browser.devedition.theme.enabled";
const PREF_THEME = "general.skins.selectedSkin";
const PREF_LWTHEME = "lightweightThemes.isThemeSelected";
const PREF_DEVTOOLS_THEME = "devtools.theme";

registerCleanupFunction(() => {
  // Set preferences back to their original values
  Services.prefs.clearUserPref(PREF_DEVEDITION_THEME);
  Services.prefs.clearUserPref(PREF_THEME);
  Services.prefs.clearUserPref(PREF_LWTHEME);
  Services.prefs.clearUserPref(PREF_DEVTOOLS_THEME);
});

function test() {
  waitForExplicitFinish();
  startTests();
}

function startTests() {
  info ("Setting browser.devedition.theme.enabled to false.");
  Services.prefs.setBoolPref(PREF_DEVEDITION_THEME, false);
  ok (!DevEdition.styleSheet, "There is no devedition style sheet when the pref is false.");

  info ("Setting browser.devedition.theme.enabled to true.");
  Services.prefs.setBoolPref(PREF_DEVEDITION_THEME, true);
  ok (DevEdition.styleSheet, "There is a devedition stylesheet when no themes are applied and pref is set.");

  info ("Adding a lightweight theme.");
  Services.prefs.setBoolPref(PREF_LWTHEME, true);
  ok (!DevEdition.styleSheet, "The devedition stylesheet has been removed when a lightweight theme is applied.");

  info ("Removing a lightweight theme.");
  Services.prefs.setBoolPref(PREF_LWTHEME, false);
  ok (DevEdition.styleSheet, "The devedition stylesheet has been added when a lightweight theme is removed.");

  // There are no listeners for the complete theme pref since applying the theme
  // requires a restart.
  info ("Setting general.skins.selectedSkin to a custom string.");
  Services.prefs.setCharPref(PREF_THEME, "custom-theme");
  ok (DevEdition.styleSheet, "The devedition stylesheet is still here when a complete theme is added.");

  info ("Resetting general.skins.selectedSkin to default value.");
  Services.prefs.clearUserPref(PREF_THEME);
  ok (DevEdition.styleSheet, "The devedition stylesheet is still here when a complete theme is removed.");

  info ("Setting browser.devedition.theme.enabled to false.");
  Services.prefs.setBoolPref(PREF_DEVEDITION_THEME, false);
  ok (!DevEdition.styleSheet, "The devedition stylesheet has been removed.");

  info ("Checking :root attributes based on devtools theme.");
  Services.prefs.setCharPref(PREF_DEVTOOLS_THEME, "light");
  is (document.documentElement.getAttribute("devtoolstheme"), "light",
    "The documentElement has an attribute based on devtools theme.");
  Services.prefs.setCharPref(PREF_DEVTOOLS_THEME, "dark");
  is (document.documentElement.getAttribute("devtoolstheme"), "dark",
    "The documentElement has an attribute based on devtools theme.");
  Services.prefs.setCharPref(PREF_DEVTOOLS_THEME, "light");
  is (document.documentElement.getAttribute("devtoolstheme"), "light",
    "The documentElement has an attribute based on devtools theme.");

  finish();
}
