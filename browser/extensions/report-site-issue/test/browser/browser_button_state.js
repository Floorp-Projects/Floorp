"use strict";

const REPORTABLE_PAGE = "http://example.com/";
const REPORTABLE_PAGE2 = "https://example.com/";
const NONREPORTABLE_PAGE = "about:mozilla";

/* Test that the Report Site Issue help menu item is enabled for http and https tabs,
   on page load, or TabSelect, and disabled for everything else. */
add_task(async function test_button_state_disabled() {
  await SpecialPowers.pushPrefEnv({ set: [[PREF_WC_REPORTER_ENABLED, true]] });

  let tab1 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    REPORTABLE_PAGE
  );
  const menu = new HelpMenuHelper();
  await menu.open();
  is(
    menu.isItemEnabled(),
    true,
    "Check that panel item is enabled for reportable schemes on tab load"
  );
  await menu.close();

  let tab2 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    NONREPORTABLE_PAGE
  );
  await menu.open();
  is(
    menu.isItemEnabled(),
    false,
    "Check that panel item is disabled for non-reportable schemes on tab load"
  );
  await menu.close();

  let tab3 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    REPORTABLE_PAGE2
  );
  await menu.open();
  is(
    menu.isItemEnabled(),
    true,
    "Check that panel item is enabled for reportable schemes on tab load"
  );
  await menu.close();

  await BrowserTestUtils.removeTab(tab1);
  await BrowserTestUtils.removeTab(tab2);
  await BrowserTestUtils.removeTab(tab3);
});
