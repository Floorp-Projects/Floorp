const REPORTABLE_PAGE = "http://example.com/";
const REPORTABLE_PAGE2 = "https://example.com/";
const NONREPORTABLE_PAGE = "about:blank";

/* Test that the Report Site Issue button is enabled for http and https tabs,
   on page load, or TabSelect, and disabled for everything else. */
add_task(function* test_button_state_disabled() {
  let tab1 = yield BrowserTestUtils.openNewForegroundTab(gBrowser, REPORTABLE_PAGE);
  yield PanelUI.show();
  is(isButtonDisabled(), false, "Check that button is enabled for reportable schemes on tab load");

  let tab2 = yield BrowserTestUtils.openNewForegroundTab(gBrowser, NONREPORTABLE_PAGE);
  is(isButtonDisabled(), true, "Check that button is disabled for non-reportable schemes on tab load");

  let tab3 = yield BrowserTestUtils.openNewForegroundTab(gBrowser, REPORTABLE_PAGE2);
  is(isButtonDisabled(), false, "Check that button is enabled for reportable schemes on tab load");


  yield BrowserTestUtils.switchTab(gBrowser, tab2);
  is(isButtonDisabled(), true, "Check that button is disabled for non-reportable schemes on TabSelect");

  yield BrowserTestUtils.switchTab(gBrowser, tab1);
  is(isButtonDisabled(), false, "Check that button is enabled for reportable schemes on TabSelect");

  yield BrowserTestUtils.removeTab(tab1);
  yield BrowserTestUtils.removeTab(tab2);
  yield BrowserTestUtils.removeTab(tab3);
});
