/*
 * Mixed Content Block frame navigates for target="_top" - Test for Bug 902350
 */


const PREF_ACTIVE = "security.mixed_content.block_active_content";
const gHttpTestRoot = "https://example.com/tests/content/base/test/";
var origBlockActive;
var gTestBrowser = null;

registerCleanupFunction(function() {
  // Set preferences back to their original values
  Services.prefs.setBoolPref(PREF_ACTIVE, origBlockActive);
});

function MixedTestsCompleted() {
  gBrowser.removeCurrentTab();
  window.focus();
  finish();
}

function test() {
  waitForExplicitFinish();

  origBlockActive = Services.prefs.getBoolPref(PREF_ACTIVE);

  Services.prefs.setBoolPref(PREF_ACTIVE, true);

  var newTab = gBrowser.addTab();
  gBrowser.selectedTab = newTab;
  gTestBrowser = gBrowser.selectedBrowser;
  newTab.linkedBrowser.stop()

  gTestBrowser.addEventListener("load", MixedTest1A, true);
  var url = gHttpTestRoot + "file_bug902350.html";
  gTestBrowser.contentWindow.location = url;
}

// Need to capture 2 loads, one for the main page and one for the iframe
function MixedTest1A() {
  gTestBrowser.removeEventListener("load", MixedTest1A, true);
  gTestBrowser.addEventListener("load", MixedTest1B, true);
}

// Find the iframe and click the link in it
function MixedTest1B() {
  gTestBrowser.removeEventListener("load", MixedTest1B, true);
  gTestBrowser.addEventListener("load", MixedTest1C, true);
  var frame = content.document.getElementById("testing_frame");
  var topTarget = frame.contentWindow.document.getElementById("topTarget");
  topTarget.click();

  // The link click should have caused a load and should not invoke the Mixed Content Blocker
  var notification = PopupNotifications.getNotification("mixed-content-blocked", gTestBrowser);
  ok(!notification, "Mixed Content Doorhanger appears when trying to navigate top");

}

function MixedTest1C() {
  gTestBrowser.removeEventListener("load", MixedTest1C, true);
  ok(gTestBrowser.contentWindow.location == "http://example.com/", "Navigating to insecure domain through target='_top' failed.")
  MixedTestsCompleted();
}


