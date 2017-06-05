const REPORTABLE_PAGE = "http://example.com/";
const REPORTABLE_PAGE2 = "https://example.com/";
const NONREPORTABLE_PAGE = "about:blank";

/* Test that the Report Site Issue button is enabled for http and https tabs,
   on page load, or TabSelect, and disabled for everything else. */
add_task(async function test_button_state_disabled() {
  CustomizableUI.addWidgetToArea("webcompat-reporter-button", "nav-bar");
  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, REPORTABLE_PAGE);
  is(isButtonDisabled(), false, "Check that button is enabled for reportable schemes on tab load");

  let tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, NONREPORTABLE_PAGE);
  is(isButtonDisabled(), true, "Check that button is disabled for non-reportable schemes on tab load");

  let tab3 = await BrowserTestUtils.openNewForegroundTab(gBrowser, REPORTABLE_PAGE2);
  is(isButtonDisabled(), false, "Check that button is enabled for reportable schemes on tab load");


  await BrowserTestUtils.switchTab(gBrowser, tab2);
  is(isButtonDisabled(), true, "Check that button is disabled for non-reportable schemes on TabSelect");

  await BrowserTestUtils.switchTab(gBrowser, tab1);
  is(isButtonDisabled(), false, "Check that button is enabled for reportable schemes on TabSelect");

  CustomizableUI.reset();
  await BrowserTestUtils.removeTab(tab1);
  await BrowserTestUtils.removeTab(tab2);
  await BrowserTestUtils.removeTab(tab3);
});
