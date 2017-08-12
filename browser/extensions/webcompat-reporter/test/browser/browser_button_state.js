const REPORTABLE_PAGE = "http://example.com/";
const REPORTABLE_PAGE2 = "https://example.com/";
const NONREPORTABLE_PAGE = "about:blank";

/* Test that the Report Site Issue button is enabled for http and https tabs,
   on page load, or TabSelect, and disabled for everything else. */
add_task(async function test_button_state_disabled() {
  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, REPORTABLE_PAGE);
  openPageActions();
  is(isButtonDisabled(), false, "Check that button is enabled for reportable schemes on tab load");

  let tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, NONREPORTABLE_PAGE);
  openPageActions();
  is(isButtonDisabled(), true, "Check that button is disabled for non-reportable schemes on tab load");

  let tab3 = await BrowserTestUtils.openNewForegroundTab(gBrowser, REPORTABLE_PAGE2);
  openPageActions();
  is(isButtonDisabled(), false, "Check that button is enabled for reportable schemes on tab load");

  await BrowserTestUtils.removeTab(tab1);
  await BrowserTestUtils.removeTab(tab2);
  await BrowserTestUtils.removeTab(tab3);
});
