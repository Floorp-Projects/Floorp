"use strict";

const REPORTABLE_PAGE = "http://example.com/";
const REPORTABLE_PAGE2 = "https://example.com/";
const NONREPORTABLE_PAGE = "about:mozilla";

/* Test that the Report Site Issue panel item is enabled for http and https tabs,
   on page load, or TabSelect, and disabled for everything else. */
add_task(async function test_button_state_disabled() {
  await SpecialPowers.pushPrefEnv({ set: [[PREF_WC_REPORTER_ENABLED, true]] });

  let tab1 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    REPORTABLE_PAGE
  );
  await openPageActions();
  is(
    await isPanelItemEnabled(),
    true,
    "Check that panel item is enabled for reportable schemes on tab load"
  );

  let tab2 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    NONREPORTABLE_PAGE
  );
  await openPageActions();
  is(
    await isPanelItemDisabled(),
    true,
    "Check that panel item is disabled for non-reportable schemes on tab load"
  );

  let tab3 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    REPORTABLE_PAGE2
  );
  await openPageActions();
  is(
    await isPanelItemEnabled(),
    true,
    "Check that panel item is enabled for reportable schemes on tab load"
  );

  await BrowserTestUtils.removeTab(tab1);
  await BrowserTestUtils.removeTab(tab2);
  await BrowserTestUtils.removeTab(tab3);
});

/* Test that the button is enabled or disabled when we expected it to be, when
   pinned to the URL bar. */
add_task(async function test_button_state_in_urlbar() {
  await SpecialPowers.pushPrefEnv({ set: [[PREF_WC_REPORTER_ENABLED, true]] });

  pinToURLBar();
  let tab1 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    REPORTABLE_PAGE
  );
  await openPageActions();
  is(
    await isURLButtonPresent(),
    true,
    "Check that urlbar icon is enabled for reportable schemes on tab load"
  );

  let tab2 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    NONREPORTABLE_PAGE
  );
  await openPageActions();
  is(
    await isURLButtonPresent(),
    false,
    "Check that urlbar icon is hidden for non-reportable schemes on tab load"
  );

  let tab3 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    REPORTABLE_PAGE2
  );
  await openPageActions();
  is(
    await isURLButtonPresent(),
    true,
    "Check that urlbar icon is enabled for reportable schemes on tab load"
  );

  unpinFromURLBar();
  await BrowserTestUtils.removeTab(tab1);
  await BrowserTestUtils.removeTab(tab2);
  await BrowserTestUtils.removeTab(tab3);
});
