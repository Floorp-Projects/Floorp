function test() {

  // ----------
  // Test setup

  waitForExplicitFinish();

  let oldOLC = FullZoom.onLocationChange;
  FullZoom.onLocationChange = function(aURI, aIsTabSwitch, aBrowser) {
    // Ignore calls that are not about tab switching on this test
    if (aIsTabSwitch)
      oldOLC.call(FullZoom, aURI, aIsTabSwitch, aBrowser);
  };

  gPrefService.setBoolPref("browser.zoom.updateBackgroundTabs", true);
  gPrefService.setBoolPref("browser.zoom.siteSpecific", true);

  let oldAPTS = FullZoom._applyPrefToSetting;
  let uri = "http://example.org/browser/browser/base/content/test/dummy_page.html";

  // ------------------------------------------------------
  // Test 1 - Zoom should not be called if URIs don't match
  FullZoom._applyPrefToSetting = function() {
    ok(false, "This should not be called");
  };
  FullZoom.onLocationChange(makeURI(uri), true);

  let tab = gBrowser.addTab();
  tab.linkedBrowser.addEventListener("load", function(event) {
    tab.linkedBrowser.removeEventListener("load", arguments.callee, true);

    // -------------------------------------------------------------------
    // Test 2 - Trigger a tab switch that should now update the zoom level
    FullZoom._applyPrefToSetting = function() {
      ok(true, "applyPrefToSetting was called");
      endTest();
    }
    gBrowser.selectedTab = tab;

  }, true); 
  tab.linkedBrowser.loadURI(uri);

  // -------------
  // Test clean-up
  function endTest() {
    FullZoom._applyPrefToSetting = oldAPTS;
    FullZoom.onLocationChange = oldOLC;
    gBrowser.removeTab(tab);

    oldAPTS = null;
    oldOLC = null;
    tab = null;

    if (gPrefService.prefHasUserValue("browser.zoom.updateBackgroundTabs"))
      gPrefService.clearUserPref("browser.zoom.updateBackgroundTabs");

    if (gPrefService.prefHasUserValue("browser.zoom.siteSpecific"))
      gPrefService.clearUserPref("browser.zoom.siteSpecific");

    finish();
  }

}

