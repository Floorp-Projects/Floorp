/*
 * Testing changes for compact themes.
 * A special stylesheet should be added to the browser.xul document
 * when the firefox-compact-light and firefox-compact-dark lightweight
 * themes are applied.
 */

const PREF_LWTHEME_USED_THEMES = "lightweightThemes.usedThemes";
const COMPACT_LIGHT_ID = "firefox-compact-light@mozilla.org";
const COMPACT_DARK_ID = "firefox-compact-dark@mozilla.org";
const SKIP_TEST = !AppConstants.INSTALL_COMPACT_THEMES;
const {LightweightThemeManager} = Components.utils.import("resource://gre/modules/LightweightThemeManager.jsm", {});

registerCleanupFunction(() => {
  // Set preferences back to their original values
  LightweightThemeManager.currentTheme = null;
  Services.prefs.clearUserPref(PREF_LWTHEME_USED_THEMES);
});

add_task(function* startTests() {
  if (SKIP_TEST) {
    ok(true, "No need to run this test since themes aren't installed");
    return;
  }

  info("Setting the current theme to null");
  LightweightThemeManager.currentTheme = null;
  ok(!CompactTheme.isStyleSheetEnabled, "There is no compact style sheet when no lw theme is applied.");

  info("Adding a lightweight theme.");
  LightweightThemeManager.currentTheme = dummyLightweightTheme("preview0");
  ok(!CompactTheme.isStyleSheetEnabled, "The compact stylesheet has been removed when a lightweight theme is applied.");

  info("Applying the dark compact theme.");
  LightweightThemeManager.currentTheme = LightweightThemeManager.getUsedTheme(COMPACT_DARK_ID);
  ok(CompactTheme.isStyleSheetEnabled, "The compact stylesheet has been added when the compact lightweight theme is applied");

  info("Applying the light compact theme.");
  LightweightThemeManager.currentTheme = LightweightThemeManager.getUsedTheme(COMPACT_LIGHT_ID);
  ok(CompactTheme.isStyleSheetEnabled, "The compact stylesheet has been added when the compact lightweight theme is applied");

  info("Adding a different lightweight theme.");
  LightweightThemeManager.currentTheme = dummyLightweightTheme("preview1");
  ok(!CompactTheme.isStyleSheetEnabled, "The compact stylesheet has been removed when a lightweight theme is applied.");

  info("Unapplying all themes.");
  LightweightThemeManager.currentTheme = null;
  ok(!CompactTheme.isStyleSheetEnabled, "There is no compact style sheet when no lw theme is applied.");
});

function dummyLightweightTheme(id) {
  return {
    id,
    name: id,
    headerURL: "resource:///chrome/browser/content/browser/defaultthemes/compact.header.png",
    iconURL: "resource:///chrome/browser/content/browser/defaultthemes/compactlight.icon.svg",
    textcolor: "red",
    accentcolor: "blue"
  };
}

add_task(function* testLightweightThemePreview() {
  if (SKIP_TEST) {
    ok(true, "No need to run this test since themes aren't installed");
    return;
  }
  info("Setting compact to current and previewing others");
  LightweightThemeManager.currentTheme = LightweightThemeManager.getUsedTheme(COMPACT_LIGHT_ID);
  ok(CompactTheme.isStyleSheetEnabled, "The compact stylesheet is enabled.");
  LightweightThemeManager.previewTheme(dummyLightweightTheme("preview0"));
  ok(!CompactTheme.isStyleSheetEnabled, "The compact stylesheet is not enabled after a lightweight theme preview.");
  LightweightThemeManager.resetPreview();
  LightweightThemeManager.previewTheme(dummyLightweightTheme("preview1"));
  ok(!CompactTheme.isStyleSheetEnabled, "The compact stylesheet is not enabled after a second lightweight theme preview.");
  LightweightThemeManager.resetPreview();
  ok(CompactTheme.isStyleSheetEnabled, "The compact stylesheet is enabled again after resetting the preview.");
  LightweightThemeManager.currentTheme = null;
  ok(!CompactTheme.isStyleSheetEnabled, "The compact stylesheet is gone after removing the current theme.");

  info("Previewing the compact theme");
  LightweightThemeManager.previewTheme(LightweightThemeManager.getUsedTheme(COMPACT_LIGHT_ID));
  ok(CompactTheme.isStyleSheetEnabled, "The compact stylesheet is enabled.");
  LightweightThemeManager.previewTheme(LightweightThemeManager.getUsedTheme(COMPACT_DARK_ID));
  ok(CompactTheme.isStyleSheetEnabled, "The compact stylesheet is enabled.");

  LightweightThemeManager.previewTheme(dummyLightweightTheme("preview2"));
  ok(!CompactTheme.isStyleSheetEnabled, "The compact stylesheet is now disabled after previewing a new sheet.");
  LightweightThemeManager.resetPreview();
  ok(!CompactTheme.isStyleSheetEnabled, "The compact stylesheet is now disabled after resetting the preview.");
});
