/*
 * Testing changes for Developer Edition theme.
 * A special stylesheet should be added to the browser.xul document
 * when the firefox-devedition@mozilla.org lightweight theme
 * is applied.
 */

const PREF_LWTHEME_USED_THEMES = "lightweightThemes.usedThemes";
const PREF_DEVTOOLS_THEME = "devtools.theme";
const {LightweightThemeManager} = Components.utils.import("resource://gre/modules/LightweightThemeManager.jsm", {});

LightweightThemeManager.clearBuiltInThemes();
LightweightThemeManager.addBuiltInTheme(dummyLightweightTheme("firefox-devedition@mozilla.org"));

registerCleanupFunction(() => {
  // Set preferences back to their original values
  LightweightThemeManager.currentTheme = null;
  Services.prefs.clearUserPref(PREF_DEVTOOLS_THEME);
  Services.prefs.clearUserPref(PREF_LWTHEME_USED_THEMES);

  LightweightThemeManager.currentTheme = null;
  LightweightThemeManager.clearBuiltInThemes();
});

add_task(function* startTests() {
  Services.prefs.setCharPref(PREF_DEVTOOLS_THEME, "dark");

  info("Setting the current theme to null");
  LightweightThemeManager.currentTheme = null;
  ok(!DevEdition.isStyleSheetEnabled, "There is no devedition style sheet when no lw theme is applied.");

  info("Adding a lightweight theme.");
  LightweightThemeManager.currentTheme = dummyLightweightTheme("preview0");
  ok(!DevEdition.isStyleSheetEnabled, "The devedition stylesheet has been removed when a lightweight theme is applied.");

  info("Applying the devedition lightweight theme.");
  let onAttributeAdded = waitForBrightTitlebarAttribute();
  LightweightThemeManager.currentTheme = LightweightThemeManager.getUsedTheme("firefox-devedition@mozilla.org");
  ok(DevEdition.isStyleSheetEnabled, "The devedition stylesheet has been added when the devedition lightweight theme is applied");
  yield onAttributeAdded;
  is(document.documentElement.getAttribute("brighttitlebarforeground"), "true",
     "The brighttitlebarforeground attribute is set on the window.");

  info("Unapplying all themes.");
  LightweightThemeManager.currentTheme = null;
  ok(!DevEdition.isStyleSheetEnabled, "There is no devedition style sheet when no lw theme is applied.");

  info("Applying the devedition lightweight theme.");
  onAttributeAdded = waitForBrightTitlebarAttribute();
  LightweightThemeManager.currentTheme = LightweightThemeManager.getUsedTheme("firefox-devedition@mozilla.org");
  ok(DevEdition.isStyleSheetEnabled, "The devedition stylesheet has been added when the devedition lightweight theme is applied");
  yield onAttributeAdded;
  ok(document.documentElement.hasAttribute("brighttitlebarforeground"),
     "The brighttitlebarforeground attribute is set on the window with dark devtools theme.");
});

add_task(function* testDevtoolsTheme() {
  info("Checking stylesheet and :root attributes based on devtools theme.");
  Services.prefs.setCharPref(PREF_DEVTOOLS_THEME, "light");
  is(document.documentElement.getAttribute("devtoolstheme"), "light",
    "The documentElement has an attribute based on devtools theme.");
  ok(DevEdition.isStyleSheetEnabled, "The devedition stylesheet is still there with the light devtools theme.");
  ok(!document.documentElement.hasAttribute("brighttitlebarforeground"),
     "The brighttitlebarforeground attribute is not set on the window with light devtools theme.");

  Services.prefs.setCharPref(PREF_DEVTOOLS_THEME, "dark");
  is(document.documentElement.getAttribute("devtoolstheme"), "dark",
    "The documentElement has an attribute based on devtools theme.");
  ok(DevEdition.isStyleSheetEnabled, "The devedition stylesheet is still there with the dark devtools theme.");
  is(document.documentElement.getAttribute("brighttitlebarforeground"), "true",
     "The brighttitlebarforeground attribute is set on the window with dark devtools theme.");

  Services.prefs.setCharPref(PREF_DEVTOOLS_THEME, "foobar");
  is(document.documentElement.getAttribute("devtoolstheme"), "light",
    "The documentElement has 'light' as a default for the devtoolstheme attribute");
  ok(DevEdition.isStyleSheetEnabled, "The devedition stylesheet is still there with the foobar devtools theme.");
  ok(!document.documentElement.hasAttribute("brighttitlebarforeground"),
     "The brighttitlebarforeground attribute is not set on the window with light devtools theme.");
});

function dummyLightweightTheme(id) {
  return {
    id: id,
    name: id,
    headerURL: "resource:///chrome/browser/content/browser/defaultthemes/devedition.header.png",
    iconURL: "resource:///chrome/browser/content/browser/defaultthemes/devedition.icon.png",
    textcolor: "red",
    accentcolor: "blue"
  };
}

add_task(function* testLightweightThemePreview() {
  info("Setting devedition to current and the previewing others");
  LightweightThemeManager.currentTheme = LightweightThemeManager.getUsedTheme("firefox-devedition@mozilla.org");
  ok(DevEdition.isStyleSheetEnabled, "The devedition stylesheet is enabled.");
  LightweightThemeManager.previewTheme(dummyLightweightTheme("preview0"));
  ok(!DevEdition.isStyleSheetEnabled, "The devedition stylesheet is not enabled after a lightweight theme preview.");
  LightweightThemeManager.resetPreview();
  LightweightThemeManager.previewTheme(dummyLightweightTheme("preview1"));
  ok(!DevEdition.isStyleSheetEnabled, "The devedition stylesheet is not enabled after a second lightweight theme preview.");
  LightweightThemeManager.resetPreview();
  ok(DevEdition.isStyleSheetEnabled, "The devedition stylesheet is enabled again after resetting the preview.");
  LightweightThemeManager.currentTheme = null;
  ok(!DevEdition.isStyleSheetEnabled, "The devedition stylesheet is gone after removing the current theme.");

  info("Previewing the devedition theme");
  LightweightThemeManager.previewTheme(LightweightThemeManager.getUsedTheme("firefox-devedition@mozilla.org"));
  ok(DevEdition.isStyleSheetEnabled, "The devedition stylesheet is enabled.");
  LightweightThemeManager.previewTheme(dummyLightweightTheme("preview2"));
  LightweightThemeManager.resetPreview();
  ok(!DevEdition.isStyleSheetEnabled, "The devedition stylesheet is now disabled after resetting the preview.");
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
