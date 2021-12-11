"use strict";

// Test the addon is cleaning up after itself when disabled.
add_task(async function test_disabled() {
  await promiseAddonEnabled();

  pinToURLBar();

  SpecialPowers.Services.prefs.setBoolPref(PREF_WC_REPORTER_ENABLED, false);
  await promisePageActionRemoved();

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "http://example.com" },
    async function() {
      await openPageActions();
      is(
        await isPanelItemPresent(),
        false,
        "Report Site Issue button is not shown on the popup panel."
      );
      is(
        await isURLButtonPresent(),
        false,
        "Report Site Issue is not shown on the url bar."
      );
    }
  );

  await promiseAddonEnabled();

  pinToURLBar();

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "http://example.com" },
    async function() {
      await openPageActions();
      is(
        await isPanelItemEnabled(),
        true,
        "Report Site Issue button is shown on the popup panel."
      );
      is(
        await isURLButtonPresent(),
        true,
        "Report Site Issue is shown on the url bar."
      );
    }
  );

  // Shut down the addon at the end,or the new instance started when we re-enabled it will "leak".
  SpecialPowers.Services.prefs.setBoolPref(PREF_WC_REPORTER_ENABLED, false);
  await promisePageActionRemoved();
});
