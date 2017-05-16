var tab;

function test() {

  // ----------
  // Test setup

  waitForExplicitFinish();

  gPrefService.setBoolPref("browser.zoom.updateBackgroundTabs", true);
  gPrefService.setBoolPref("browser.zoom.siteSpecific", true);

  let uri = "http://example.org/browser/browser/base/content/test/general/dummy_page.html";

  (async function() {
    tab = BrowserTestUtils.addTab(gBrowser);
    await FullZoomHelper.load(tab, uri);

    // -------------------------------------------------------------------
    // Test - Trigger a tab switch that should update the zoom level
    await FullZoomHelper.selectTabAndWaitForLocationChange(tab);
    ok(true, "applyPrefToSetting was called");
  })().then(endTest, FullZoomHelper.failAndContinue(endTest));
}

// -------------
// Test clean-up
function endTest() {
  (async function() {
    await FullZoomHelper.removeTabAndWaitForLocationChange(tab);

    tab = null;

    if (gPrefService.prefHasUserValue("browser.zoom.updateBackgroundTabs"))
      gPrefService.clearUserPref("browser.zoom.updateBackgroundTabs");

    if (gPrefService.prefHasUserValue("browser.zoom.siteSpecific"))
      gPrefService.clearUserPref("browser.zoom.siteSpecific");

    finish();
  })();
}
