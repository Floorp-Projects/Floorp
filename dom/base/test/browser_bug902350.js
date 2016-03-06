/*
 * Mixed Content Block frame navigates for target="_top" - Test for Bug 902350
 */


const PREF_ACTIVE = "security.mixed_content.block_active_content";
const gHttpTestRoot = "https://example.com/tests/dom/base/test/";
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

  BrowserTestUtils.browserLoaded(gTestBrowser, true /*includeSubFrames*/).then(MixedTest1A);
  var url = gHttpTestRoot + "file_bug902350.html";
  gTestBrowser.loadURI(url);
}

// Need to capture 2 loads, one for the main page and one for the iframe
function MixedTest1A() {
  BrowserTestUtils.browserLoaded(gTestBrowser, true /*includeSubFrames*/).then(MixedTest1B);
}

// Find the iframe and click the link in it
function MixedTest1B() {
  BrowserTestUtils.browserLoaded(gTestBrowser).then(MixedTest1C);

  ContentTask.spawn(gTestBrowser, null, function() {
    var frame = content.document.getElementById("testing_frame");
    var topTarget = frame.contentWindow.document.getElementById("topTarget");
    topTarget.click();
  });

  // The link click should have caused a load and should not invoke the Mixed Content Blocker
  let {gIdentityHandler} = gTestBrowser.ownerGlobal;
  ok (!gIdentityHandler._identityBox.classList.contains("mixedActiveBlocked"),
      "Mixed Content Doorhanger did not appear when trying to navigate top");
}

function MixedTest1C() {
  ContentTask.spawn(gTestBrowser, null, function() {
    Assert.equal(content.location.href, "http://example.com/",
      "Navigating to insecure domain through target='_top' failed.")
  }).then(MixedTestsCompleted);
}

