/*
 * Testing changes for Developer Edition theme.
 * A special stylesheet should be added to the browser.xul document
 * when browser.devedition.theme.enabled is set to true and no themes
 * are applied.
 */

const PREF_DEVEDITION_THEME = "browser.devedition.theme.enabled";
const PREF_LWTHEME = "lightweightThemes.selectedThemeID";
const PREF_LWTHEME_USED_THEMES = "lightweightThemes.usedThemes";
const PREF_DEVTOOLS_THEME = "devtools.theme";
const {LightweightThemeManager} = Components.utils.import("resource://gre/modules/LightweightThemeManager.jsm", {});

registerCleanupFunction(() => {
  // Set preferences back to their original values
  LightweightThemeManager.currentTheme = null;
  Services.prefs.clearUserPref(PREF_DEVEDITION_THEME);
  Services.prefs.clearUserPref(PREF_LWTHEME);
  Services.prefs.clearUserPref(PREF_DEVTOOLS_THEME);
  Services.prefs.clearUserPref(PREF_LWTHEME_USED_THEMES);
});

add_task(function* startTests() {
  Services.prefs.setCharPref(PREF_DEVTOOLS_THEME, "dark");

  info ("Setting browser.devedition.theme.enabled to false.");
  Services.prefs.setBoolPref(PREF_DEVEDITION_THEME, false);
  ok (!DevEdition.styleSheet, "There is no devedition style sheet when the pref is false.");

  info ("Setting browser.devedition.theme.enabled to true.");
  Services.prefs.setBoolPref(PREF_DEVEDITION_THEME, true);
  ok (DevEdition.styleSheet, "There is a devedition stylesheet when no themes are applied and pref is set.");

  info ("Adding a lightweight theme.");
  LightweightThemeManager.currentTheme = dummyLightweightTheme("preview0");
  ok (!DevEdition.styleSheet, "The devedition stylesheet has been removed when a lightweight theme is applied.");

  info ("Removing a lightweight theme.");
  let onAttributeAdded = waitForBrightTitlebarAttribute();
  LightweightThemeManager.currentTheme = null;
  ok (DevEdition.styleSheet, "The devedition stylesheet has been added when a lightweight theme is removed.");
  yield onAttributeAdded;

  is (document.documentElement.getAttribute("brighttitlebarforeground"), "true",
     "The brighttitlebarforeground attribute is set on the window.");

  info ("Setting browser.devedition.theme.enabled to false.");
  Services.prefs.setBoolPref(PREF_DEVEDITION_THEME, false);
  ok (!DevEdition.styleSheet, "The devedition stylesheet has been removed.");

  ok (!document.documentElement.hasAttribute("brighttitlebarforeground"),
     "The brighttitlebarforeground attribute is not set on the window after devedition.theme is false.");
});

add_task(function* testDevtoolsTheme() {
  info ("Checking that Australis is shown when the light devtools theme is applied.");

  let onAttributeAdded = waitForBrightTitlebarAttribute();
  Services.prefs.setBoolPref(PREF_DEVEDITION_THEME, true);
  ok (DevEdition.styleSheet, "The devedition stylesheet exists.");
  yield onAttributeAdded;
  ok (document.documentElement.hasAttribute("brighttitlebarforeground"),
     "The brighttitlebarforeground attribute is set on the window with dark devtools theme.");

  info ("Checking stylesheet and :root attributes based on devtools theme.");
  Services.prefs.setCharPref(PREF_DEVTOOLS_THEME, "light");
  is (document.documentElement.getAttribute("devtoolstheme"), "light",
    "The documentElement has an attribute based on devtools theme.");
  ok (DevEdition.styleSheet, "The devedition stylesheet is still there with the light devtools theme.");
  ok (!document.documentElement.hasAttribute("brighttitlebarforeground"),
     "The brighttitlebarforeground attribute is not set on the window with light devtools theme.");

  Services.prefs.setCharPref(PREF_DEVTOOLS_THEME, "dark");
  is (document.documentElement.getAttribute("devtoolstheme"), "dark",
    "The documentElement has an attribute based on devtools theme.");
  ok (DevEdition.styleSheet, "The devedition stylesheet is still there with the dark devtools theme.");
  is (document.documentElement.getAttribute("brighttitlebarforeground"), "true",
     "The brighttitlebarforeground attribute is set on the window with dark devtools theme.");

  Services.prefs.setCharPref(PREF_DEVTOOLS_THEME, "foobar");
  is (document.documentElement.getAttribute("devtoolstheme"), "light",
    "The documentElement has 'light' as a default for the devtoolstheme attribute");
  ok (DevEdition.styleSheet, "The devedition stylesheet is still there with the foobar devtools theme.");
  ok (!document.documentElement.hasAttribute("brighttitlebarforeground"),
     "The brighttitlebarforeground attribute is not set on the window with light devtools theme.");
});

function dummyLightweightTheme(id) {
  return {
    id: id,
    name: id,
    headerURL: "resource:///chrome/browser/content/browser/defaultthemes/1.header.jpg",
    iconURL: "resource:///chrome/browser/content/browser/defaultthemes/1.icon.jpg",
    textcolor: "red",
    accentcolor: "blue"
  };
}

add_task(function* testLightweightThemePreview() {
  info ("Turning the pref on, then previewing lightweight themes");
  Services.prefs.setBoolPref(PREF_DEVEDITION_THEME, true);
  ok (DevEdition.styleSheet, "The devedition stylesheet is enabled.");
  LightweightThemeManager.previewTheme(dummyLightweightTheme("preview0"));
  ok (!DevEdition.styleSheet, "The devedition stylesheet is not enabled after a lightweight theme preview.");
  LightweightThemeManager.resetPreview();
  LightweightThemeManager.previewTheme(dummyLightweightTheme("preview1"));
  ok (!DevEdition.styleSheet, "The devedition stylesheet is not enabled after a second lightweight theme preview.");
  LightweightThemeManager.resetPreview();
  ok (DevEdition.styleSheet, "The devedition stylesheet is enabled again after resetting the preview.");

  info ("Turning the pref on, then previewing a theme, turning it off and resetting the preview");
  Services.prefs.setBoolPref(PREF_DEVEDITION_THEME, true);
  ok (DevEdition.styleSheet, "The devedition stylesheet is enabled.");
  LightweightThemeManager.previewTheme(dummyLightweightTheme("preview2"));
  ok (!DevEdition.styleSheet, "The devedition stylesheet is not enabled after a lightweight theme preview.");
  Services.prefs.setBoolPref(PREF_DEVEDITION_THEME, false);
  ok (!DevEdition.styleSheet, "The devedition stylesheet is not enabled after pref is turned off.");
  LightweightThemeManager.resetPreview();
  ok (!DevEdition.styleSheet, "The devedition stylesheet is still disabled after resetting the preview.");

  info ("Turning the pref on, then previewing the default theme, turning it off and resetting the preview");
  Services.prefs.setBoolPref(PREF_DEVEDITION_THEME, true);
  ok (DevEdition.styleSheet, "The devedition stylesheet is enabled.");
  LightweightThemeManager.previewTheme(null);
  ok (DevEdition.styleSheet, "The devedition stylesheet is still enabled after the default theme is applied.");
  LightweightThemeManager.resetPreview();
  ok (DevEdition.styleSheet, "The devedition stylesheet is still enabled after resetting the preview.");
});

// Use a mutation observer to wait for the brighttitlebarforeground
// attribute to change.  Using this instead of waiting for the load
// event on the DevEdition styleSheet.
function waitForBrightTitlebarAttribute() {
  return new Promise((resolve, reject) => {
    let mutationObserver = new MutationObserver(function (mutations) {
      for (let mutation of mutations) {
        if (mutation.attributeName == "brighttitlebarforeground") {
          mutationObserver.disconnect();
          resolve();
        }
      }
    });
    mutationObserver.observe(document.documentElement, { attributes: true });
  });
}
