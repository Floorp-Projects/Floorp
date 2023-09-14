/*
 * Description of the test:
 *   Ensure that CORS warnings are printed to the web console.
 *
 *   This test uses the same tests as the plain mochitest, but needs access to
 *   the console.
 */
"use strict";

function console_observer(subject, topic, data) {
  var message = subject.wrappedJSObject.arguments[0];
  ok(false, message);
}

var webconsole = null;
var messages_seen = 0;
var expected_messages = 50;

function on_new_message(msgObj) {
  let text = msgObj.message;

  if (text.match("Cross-Origin Request Blocked:")) {
    ok(true, "message is: " + text);
    messages_seen++;
  }
}

async function do_cleanup() {
  Services.console.unregisterListener(on_new_message);
  await unsetCookiePref();
}

/**
 * Set e10s related preferences in the test environment.
 * @return {Promise} promise that resolves when preferences are set.
 */
function setCookiePref() {
  return new Promise(resolve =>
    // accept all cookies so that the CORS requests will send the right cookies
    SpecialPowers.pushPrefEnv(
      {
        set: [["network.cookie.cookieBehavior", 0]],
      },
      resolve
    )
  );
}

/**
 * Unset e10s related preferences in the test environment.
 * @return {Promise} promise that resolves when preferences are unset.
 */
function unsetCookiePref() {
  return new Promise(resolve => {
    SpecialPowers.popPrefEnv(resolve);
  });
}

//jscs:disable
add_task(async function () {
  //jscs:enable
  // A longer timeout is necessary for this test than the plain mochitests
  // due to opening a new tab with the web console.
  requestLongerTimeout(4);
  registerCleanupFunction(do_cleanup);
  await setCookiePref();
  Services.console.registerListener(on_new_message);

  let test_uri =
    "http://mochi.test:8888/browser/dom/security/test/cors/file_cors_logging_test.html";

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );

  BrowserTestUtils.startLoadingURIString(gBrowser, test_uri);

  await BrowserTestUtils.waitForLocationChange(
    gBrowser,
    test_uri + "#finished"
  );

  // Different OS combinations
  ok(messages_seen > 0, "Saw " + messages_seen + " messages.");

  messages_seen = 0;
  let test_two_uri =
    "http://mochi.test:8888/browser/dom/security/test/cors/file_bug1456721.html";
  BrowserTestUtils.startLoadingURIString(gBrowser, test_two_uri);

  await BrowserTestUtils.waitForLocationChange(
    gBrowser,
    test_two_uri + "#finishedTestTwo"
  );
  await BrowserTestUtils.waitForCondition(() => messages_seen > 0);

  ok(messages_seen > 0, "Saw " + messages_seen + " messages.");

  BrowserTestUtils.removeTab(tab);
});
