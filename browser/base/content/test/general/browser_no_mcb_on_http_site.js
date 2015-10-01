/*
 * Description of the Tests for
 *  - Bug 909920 - Mixed content warning should not show on a HTTP site
 *
 * Description of the tests:
 *   Test 1:
 *     1) Load an http page
 *     2) The page includes a css file using https
 *     3) The css file loads an |IMAGE| << over http
 *
 *   Test 2:
 *     1) Load an http page
 *     2) The page includes a css file using https
 *     3) The css file loads a |FONT| over http
 *
 *   Test 3:
 *     1) Load an http page
 *     2) The page includes a css file using https
 *     3) The css file imports (@import) another css file using http
 *     3) The imported css file loads a |FONT| over http
*
 * Since the top-domain is >> NOT << served using https, the MCB
 * should >> NOT << trigger a warning.
 */

const PREF_ACTIVE = "security.mixed_content.block_active_content";
const PREF_DISPLAY = "security.mixed_content.block_display_content";

const gHttpTestRoot = "http://example.com/browser/browser/base/content/test/general/";

var origBlockActive, origBlockDisplay;
var gTestBrowser = null;

registerCleanupFunction(function() {
  // Set preferences back to their original values
  Services.prefs.setBoolPref(PREF_ACTIVE, origBlockActive);
  Services.prefs.setBoolPref(PREF_DISPLAY, origBlockDisplay);
});

function cleanUpAfterTests() {
  gBrowser.removeCurrentTab();
  window.focus();
  finish();
}

function waitForCondition(condition, nextTest, errorMsg, okMsg) {
  var tries = 0;
  var interval = setInterval(function() {
    if (tries >= 30) {
      ok(false, errorMsg);
      moveOn();
    }
    if (condition()) {
      ok(true, okMsg)
      moveOn();
    }
    tries++;
  }, 100);
  var moveOn = function() {
    clearInterval(interval); nextTest();
  };
}

//------------- TEST 1 -----------------------------------------

function test1A() {
  gTestBrowser.removeEventListener("load", test1A, true);

  var expected = "Verifying MCB does not trigger warning/error for an http page ";
  expected += "with https css that includes http image";
  waitForCondition(
    () => content.document.getElementById('testDiv').innerHTML == expected,
    test1B, "Error: Waited too long for status in Test 1!",
    "OK: Expected result in innerHTML!");
}

function test1B() {
  // set up test 2
  gTestBrowser.addEventListener("load", test2A, true);
  var url = gHttpTestRoot + "test_no_mcb_on_http_site_font.html";
  gTestBrowser.contentWindow.location = url;
}

//------------- TEST 2 -----------------------------------------

function test2A() {
  gTestBrowser.removeEventListener("load", test2A, true);

  var expected = "Verifying MCB does not trigger warning/error for an http page ";
  expected += "with https css that includes http font";
  waitForCondition(
    () => content.document.getElementById('testDiv').innerHTML == expected,
    test2B, "Error: Waited too long for status in Test 2!",
    "OK: Expected result in innerHTML!");
}

function test2B() {
  // set up test 3
  gTestBrowser.addEventListener("load", test3, true);
  var url = gHttpTestRoot + "test_no_mcb_on_http_site_font2.html";
  gTestBrowser.contentWindow.location = url;
}

//------------- TEST 3 -----------------------------------------

function test3() {
  gTestBrowser.removeEventListener("load", test3, true);

  var expected = "Verifying MCB does not trigger warning/error for an http page "
  expected += "with https css that imports another http css which includes http font";
  waitForCondition(
    () => content.document.getElementById('testDiv').innerHTML == expected,
    cleanUpAfterTests, "Error: Waited too long for status in Test 3!",
    "OK: Expected result in innerHTML!");
}

//------------------------------------------------------

function test() {
  // Performing async calls, e.g. 'onload', we have to wait till all of them finished
  waitForExplicitFinish();

  // Store original preferences so we can restore settings after testing
  origBlockActive = Services.prefs.getBoolPref(PREF_ACTIVE);
  origBlockDisplay = Services.prefs.getBoolPref(PREF_DISPLAY);

  Services.prefs.setBoolPref(PREF_ACTIVE, true);
  Services.prefs.setBoolPref(PREF_DISPLAY, true);

  var newTab = gBrowser.addTab();
  gBrowser.selectedTab = newTab;
  gTestBrowser = gBrowser.selectedBrowser;
  newTab.linkedBrowser.stop();

  gTestBrowser.addEventListener("load", test1A, true);
  var url = gHttpTestRoot + "test_no_mcb_on_http_site_img.html";
  gTestBrowser.contentWindow.location = url;
}
