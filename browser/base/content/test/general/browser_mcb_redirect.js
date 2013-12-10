/*
 * Description of the Tests for
 *  - Bug 418354 - Call Mixed content blocking on redirects
 *
 * 1. Load a script over https inside an https page
 *    - the server responds with a 302 redirect to a >> HTTP << script
 *    - the doorhanger should appear!
 *
 * 2. Load a script over https inside an http page
 *    - the server responds with a 302 redirect to a >> HTTP << script
 *    - the doorhanger should not appear!
 */

const PREF_ACTIVE = "security.mixed_content.block_active_content";
const gHttpsTestRoot = "https://example.com/browser/browser/base/content/test/general/";
const gHttpTestRoot = "http://example.com/browser/browser/base/content/test/general/";

let origBlockActive;
var gTestBrowser = null;

//------------------------ Helper Functions ---------------------

registerCleanupFunction(function() {
  // Set preferences back to their original values
  Services.prefs.setBoolPref(PREF_ACTIVE, origBlockActive);
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

//------------------------ Test 1 ------------------------------

function test1() {
  gTestBrowser.addEventListener("load", checkPopUpNotificationsForTest1, true);
  var url = gHttpsTestRoot + "test_mcb_redirect.html"
  gTestBrowser.contentWindow.location = url;
}

function checkPopUpNotificationsForTest1() {
  gTestBrowser.removeEventListener("load", checkPopUpNotificationsForTest1, true);

  var notification = PopupNotifications.getNotification("bad-content", gTestBrowser.selectedBrowser);
  ok(notification, "OK: Mixed Content Doorhanger appeared in Test1!");

  var expected = "script blocked";
  waitForCondition(
    function() content.document.getElementById('mctestdiv').innerHTML == expected,
    test2, "Error: Waited too long for status in Test 1!",
    "OK: Expected result in innerHTML for Test1!");
}

//------------------------ Test 2 ------------------------------

function test2() {
  gTestBrowser.addEventListener("load", checkPopUpNotificationsForTest2, true);
  var url = gHttpTestRoot + "test_mcb_redirect.html"
  gTestBrowser.contentWindow.location = url;
}

function checkPopUpNotificationsForTest2() {
  gTestBrowser.removeEventListener("load", checkPopUpNotificationsForTest2, true);

  var notification = PopupNotifications.getNotification("bad-content", gTestBrowser.selectedBrowser);
  ok(!notification, "OK: Mixed Content Doorhanger did not appear in 2!");

  var expected = "script executed";
  waitForCondition(
    function() content.document.getElementById('mctestdiv').innerHTML == expected,
    cleanUpAfterTests, "Error: Waited too long for status in Test 2!",
    "OK: Expected result in innerHTML for Test2!");
}

//------------------------ SETUP ------------------------------

function test() {
  // Performing async calls, e.g. 'onload', we have to wait till all of them finished
  waitForExplicitFinish();

  // Store original preferences so we can restore settings after testing
  origBlockActive = Services.prefs.getBoolPref(PREF_ACTIVE);
  Services.prefs.setBoolPref(PREF_ACTIVE, true);

  var newTab = gBrowser.addTab();
  gBrowser.selectedTab = newTab;
  gTestBrowser = gBrowser.selectedBrowser;
  newTab.linkedBrowser.stop();

  executeSoon(test1);
}
