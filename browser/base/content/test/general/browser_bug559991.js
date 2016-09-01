var tab;

function test() {

  // ----------
  // Test setup

  waitForExplicitFinish();

  gPrefService.setBoolPref("browser.zoom.updateBackgroundTabs", true);
  gPrefService.setBoolPref("browser.zoom.siteSpecific", true);

  let uri = "http://example.org/browser/browser/base/content/test/general/dummy_page.html";

  Task.spawn(function* () {
    tab = gBrowser.addTab();
    yield FullZoomHelper.load(tab, uri);

    // -------------------------------------------------------------------
    // Test - Trigger a tab switch that should update the zoom level
    yield FullZoomHelper.selectTabAndWaitForLocationChange(tab);
    ok(true, "applyPrefToSetting was called");
  }).then(endTest, FullZoomHelper.failAndContinue(endTest));
}

// -------------
// Test clean-up
function endTest() {
  Task.spawn(function* () {
    yield FullZoomHelper.removeTabAndWaitForLocationChange(tab);

    tab = null;

    if (gPrefService.prefHasUserValue("browser.zoom.updateBackgroundTabs"))
      gPrefService.clearUserPref("browser.zoom.updateBackgroundTabs");

    if (gPrefService.prefHasUserValue("browser.zoom.siteSpecific"))
      gPrefService.clearUserPref("browser.zoom.siteSpecific");

    finish();
  });
}
