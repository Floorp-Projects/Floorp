/*
 * Testing changes for compact themes.
 * A special stylesheet should be added to the browser.xul document
 * when the firefox-compact-light and firefox-compact-dark lightweight
 * themes are applied.
 */

const DEFAULT_THEME = "default-theme@mozilla.org";
const COMPACT_LIGHT_ID = "firefox-compact-light@mozilla.org";
const COMPACT_DARK_ID = "firefox-compact-dark@mozilla.org";
const {AddonManager} = ChromeUtils.import("resource://gre/modules/AddonManager.jsm");

async function selectTheme(id) {
  let theme = await AddonManager.getAddonByID(id || DEFAULT_THEME);
  await theme.enable();
}

registerCleanupFunction(() => {
  // Set preferences back to their original values
  return selectTheme(null);
});

function tick() {
  return new Promise(SimpleTest.executeSoon);
}

add_task(async function startTests() {
  info("Setting the current theme to null");
  await selectTheme(null);
  await tick();
  ok(!CompactTheme.isStyleSheetEnabled, "There is no compact style sheet when no lw theme is applied.");

  info("Applying the dark compact theme.");
  await selectTheme(COMPACT_DARK_ID);
  await tick();
  ok(CompactTheme.isStyleSheetEnabled, "The compact stylesheet has been added when the compact lightweight theme is applied");

  info("Applying the light compact theme.");
  await selectTheme(COMPACT_LIGHT_ID);
  await tick();
  ok(CompactTheme.isStyleSheetEnabled, "The compact stylesheet has been added when the compact lightweight theme is applied");

  info("Unapplying all themes.");
  await selectTheme(null);
  await tick();
  ok(!CompactTheme.isStyleSheetEnabled, "There is no compact style sheet when no lw theme is applied.");
});
