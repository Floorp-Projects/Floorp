/*
 * Description of the Tests for
 *  - Bug 418354 - Call Mixed content blocking on redirects
 *
 * Single redirect script tests
 * 1. Load a script over https inside an https page
 *    - the server responds with a 302 redirect to a >> HTTP << script
 *    - the doorhanger should appear!
 *
 * 2. Load a script over https inside an http page
 *    - the server responds with a 302 redirect to a >> HTTP << script
 *    - the doorhanger should not appear!
 *
 * Single redirect image tests
 * 3. Load an image over https inside an https page
 *    - the server responds with a 302 redirect to a >> HTTP << image
 *    - the image should not load
 *
 * 4. Load an image over https inside an http page
 *    - the server responds with a 302 redirect to a >> HTTP << image
 *    - the image should load and get cached
 *
 * Single redirect cached image tests
 * 5. Using offline mode to ensure we hit the cache, load a cached image over
 *    https inside an http page
 *    - the server would have responded with a 302 redirect to a >> HTTP <<
 *      image, but instead we try to use the cached image.
 *    - the image should load
 *
 * 6. Using offline mode to ensure we hit the cache, load a cached image over
 *    https inside an https page
 *    - the server would have responded with a 302 redirect to a >> HTTP <<
 *      image, but instead we try to use the cached image.
 *    - the image should not load
 *
 * Double redirect image test
 * 7. Load an image over https inside an http page
 *    - the server responds with a 302 redirect to a >> HTTP << server
 *    - the HTTP server responds with a 302 redirect to a >> HTTPS << image
 *    - the image should load and get cached
 *
 * Double redirect cached image tests
 * 8. Using offline mode to ensure we hit the cache, load a cached image over
 *    https inside an http page
 *    - the image would have gone through two redirects: HTTPS->HTTP->HTTPS,
 *      but instead we try to use the cached image.
 *    - the image should load
 *
 * 9. Using offline mode to ensure we hit the cache, load a cached image over
 *    https inside an https page
 *    - the image would have gone through two redirects: HTTPS->HTTP->HTTPS,
 *      but instead we try to use the cached image.
 *    - the image should not load
 */

const PREF_ACTIVE = "security.mixed_content.block_active_content";
const PREF_DISPLAY = "security.mixed_content.block_display_content";
const gHttpsTestRoot = "https://example.com/browser/browser/base/content/test/general/";
const gHttpTestRoot = "http://example.com/browser/browser/base/content/test/general/";

var origBlockActive;
var origBlockDisplay;
var gTestBrowser = null;

//------------------------ Helper Functions ---------------------

registerCleanupFunction(function() {
  // Set preferences back to their original values
  Services.prefs.setBoolPref(PREF_ACTIVE, origBlockActive);
  Services.prefs.setBoolPref(PREF_DISPLAY, origBlockDisplay);

  // Make sure we are online again
  Services.io.offline = false;
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
  gTestBrowser.addEventListener("load", checkUIForTest1, true);
  var url = gHttpsTestRoot + "test_mcb_redirect.html"
  gTestBrowser.contentWindow.location = url;
}

function checkUIForTest1() {
  gTestBrowser.removeEventListener("load", checkUIForTest1, true);

  assertMixedContentBlockingState(gTestBrowser, {activeLoaded: false, activeBlocked: true, passiveLoaded: false});

  var expected = "script blocked";
  waitForCondition(
    () => content.document.getElementById('mctestdiv').innerHTML == expected,
    test2, "Error: Waited too long for status in Test 1!",
    "OK: Expected result in innerHTML for Test1!");
}

//------------------------ Test 2 ------------------------------

function test2() {
  gTestBrowser.addEventListener("load", checkUIForTest2, true);
  var url = gHttpTestRoot + "test_mcb_redirect.html"
  gTestBrowser.contentWindow.location = url;
}

function checkUIForTest2() {
  gTestBrowser.removeEventListener("load", checkUIForTest2, true);

  assertMixedContentBlockingState(gTestBrowser, {activeLoaded: false, activeBlocked: false, passiveLoaded: false});

  var expected = "script executed";
  waitForCondition(
    () => content.document.getElementById('mctestdiv').innerHTML == expected,
    test3, "Error: Waited too long for status in Test 2!",
    "OK: Expected result in innerHTML for Test2!");
}

//------------------------ Test 3 ------------------------------
// HTTPS page loading insecure image
function test3() {
  gTestBrowser.addEventListener("load", checkLoadEventForTest3, true);
  var url = gHttpsTestRoot + "test_mcb_redirect_image.html"
  gTestBrowser.contentWindow.location = url;
}

function checkLoadEventForTest3() {
  gTestBrowser.removeEventListener("load", checkLoadEventForTest3, true);

  var expected = "image blocked"
  waitForCondition(
    () => content.document.getElementById('mctestdiv').innerHTML == expected,
    test4, "Error: Waited too long for status in Test 3!",
    "OK: Expected result in innerHTML for Test3!");
}

//------------------------ Test 4 ------------------------------
// HTTP page loading insecure image
function test4() {
  gTestBrowser.addEventListener("load", checkLoadEventForTest4, true);
  var url = gHttpTestRoot + "test_mcb_redirect_image.html"
  gTestBrowser.contentWindow.location = url;
}

function checkLoadEventForTest4() {
  gTestBrowser.removeEventListener("load", checkLoadEventForTest4, true);

  var expected = "image loaded"
  waitForCondition(
    () => content.document.getElementById('mctestdiv').innerHTML == expected,
    test5, "Error: Waited too long for status in Test 4!",
    "OK: Expected result in innerHTML for Test4!");
}

//------------------------ Test 5 ------------------------------
// HTTP page laoding insecure cached image
// Assuming test 4 succeeded, the image has already been loaded once
// and hence should be cached per the sjs cache-control header
// Going into offline mode to ensure we are loading from the cache.
function test5() {
  gTestBrowser.addEventListener("load", checkLoadEventForTest5, true);
  // Go into offline mode
  Services.io.offline = true;
  var url = gHttpTestRoot + "test_mcb_redirect_image.html"
  gTestBrowser.contentWindow.location = url;
}

function checkLoadEventForTest5() {
  gTestBrowser.removeEventListener("load", checkLoadEventForTest5, true);

  var expected = "image loaded"
  waitForCondition(
    () => content.document.getElementById('mctestdiv').innerHTML == expected,
    test6, "Error: Waited too long for status in Test 5!",
    "OK: Expected result in innerHTML for Test5!");
  // Go back online
  Services.io.offline = false;
}

//------------------------ Test 6 ------------------------------
// HTTPS page loading insecure cached image
// Assuming test 4 succeeded, the image has already been loaded once
// and hence should be cached per the sjs cache-control header
// Going into offline mode to ensure we are loading from the cache.
function test6() {
  gTestBrowser.addEventListener("load", checkLoadEventForTest6, true);
  // Go into offline mode
  Services.io.offline = true;
  var url = gHttpsTestRoot + "test_mcb_redirect_image.html"
  gTestBrowser.contentWindow.location = url;
}

function checkLoadEventForTest6() {
  gTestBrowser.removeEventListener("load", checkLoadEventForTest6, true);

  var expected = "image blocked"
  waitForCondition(
    () => content.document.getElementById('mctestdiv').innerHTML == expected,
    test7, "Error: Waited too long for status in Test 6!",
    "OK: Expected result in innerHTML for Test6!");
  // Go back online
  Services.io.offline = false;
}

//------------------------ Test 7 ------------------------------
// HTTP page loading insecure image that went through a double redirect
function test7() {
  gTestBrowser.addEventListener("load", checkLoadEventForTest7, true);
  var url = gHttpTestRoot + "test_mcb_double_redirect_image.html"
  gTestBrowser.contentWindow.location = url;
}

function checkLoadEventForTest7() {
  gTestBrowser.removeEventListener("load", checkLoadEventForTest7, true);

  var expected = "image loaded"
  waitForCondition(
    () => content.document.getElementById('mctestdiv').innerHTML == expected,
    test8, "Error: Waited too long for status in Test 7!",
    "OK: Expected result in innerHTML for Test7!");
}

//------------------------ Test 8 ------------------------------
// HTTP page loading insecure cached image that went through a double redirect
// Assuming test 7 succeeded, the image has already been loaded once
// and hence should be cached per the sjs cache-control header
// Going into offline mode to ensure we are loading from the cache.
function test8() {
  gTestBrowser.addEventListener("load", checkLoadEventForTest8, true);
  // Go into offline mode
  Services.io.offline = true;
  var url = gHttpTestRoot + "test_mcb_double_redirect_image.html"
  gTestBrowser.contentWindow.location = url;
}

function checkLoadEventForTest8() {
  gTestBrowser.removeEventListener("load", checkLoadEventForTest8, true);

  var expected = "image loaded"
  waitForCondition(
    () => content.document.getElementById('mctestdiv').innerHTML == expected,
    test9, "Error: Waited too long for status in Test 8!",
    "OK: Expected result in innerHTML for Test8!");
  // Go back online
  Services.io.offline = false;
}

//------------------------ Test 9 ------------------------------
// HTTPS page loading insecure cached image that went through a double redirect
// Assuming test 7 succeeded, the image has already been loaded once
// and hence should be cached per the sjs cache-control header
// Going into offline mode to ensure we are loading from the cache.
function test9() {
  gTestBrowser.addEventListener("load", checkLoadEventForTest9, true);
  // Go into offline mode
  Services.io.offline = true;
  var url = gHttpsTestRoot + "test_mcb_double_redirect_image.html"
  gTestBrowser.contentWindow.location = url;
}

function checkLoadEventForTest9() {
  gTestBrowser.removeEventListener("load", checkLoadEventForTest9, true);

  var expected = "image blocked"
  waitForCondition(
    () => content.document.getElementById('mctestdiv').innerHTML == expected,
    cleanUpAfterTests, "Error: Waited too long for status in Test 9!",
    "OK: Expected result in innerHTML for Test9!");
  // Go back online
  Services.io.offline = false;
}

//------------------------ SETUP ------------------------------

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

  executeSoon(test1);
}
