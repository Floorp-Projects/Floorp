"use strict";

// Test the addon is cleaning up after itself when disabled.
add_task(async function test_disabled() {
  await promiseAddonEnabled();

  SpecialPowers.Services.prefs.setBoolPref(PREF_WC_REPORTER_ENABLED, false);

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "http://example.com" },
    async function() {
      const menu = new HelpMenuHelper();
      await menu.open();
      is(
        menu.isItemHidden(),
        true,
        "Report Site Issue help menu item is hidden."
      );
      await menu.close();
    }
  );

  await promiseAddonEnabled();

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "http://example.com" },
    async function() {
      const menu = new HelpMenuHelper();
      await menu.open();
      is(
        await menu.isItemHidden(),
        false,
        "Report Site Issue help menu item is visible."
      );
      await menu.close();
    }
  );

  // Shut down the addon at the end,or the new instance started when we re-enabled it will "leak".
  SpecialPowers.Services.prefs.setBoolPref(PREF_WC_REPORTER_ENABLED, false);
});
