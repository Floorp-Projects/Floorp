/*
 * Description of the test:
 *   Ensure that CORS warnings are printed to the web console.
 *
 *   This test uses the same tests as the plain mochitest, but needs access to
 *   the console.
 */
'use strict';

function console_observer(subject, topic, data) {
  var message = subject.wrappedJSObject.arguments[0];
  ok(false, message);
};

var webconsole = null;
var messages_seen = 0;
var expected_messages = 50;

function on_new_message(event, new_messages) {
  for (let message of new_messages) {
    let elem = message.node;
    let text = elem.textContent;
    if (text.match('Cross-Origin Request Blocked:')) {
      ok(true, "message is: " + text);
      messages_seen++;
    }
  }
}

function* do_cleanup() {
  if (webconsole) {
    webconsole.ui.off("new-messages", on_new_message);
  }
  yield unsetCookiePref();
}

/**
 * Set e10s related preferences in the test environment.
 * @return {Promise} promise that resolves when preferences are set.
 */
function setCookiePref() {
  return new Promise(resolve =>
    // accept all cookies so that the CORS requests will send the right cookies
    SpecialPowers.pushPrefEnv({
      set: [
        ["network.cookie.cookieBehavior", 0],
      ]
    }, resolve));
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
add_task(function*() {
  //jscs:enable
  // A longer timeout is necessary for this test than the plain mochitests
  // due to opening a new tab with the web console.
  requestLongerTimeout(4);
  registerCleanupFunction(do_cleanup);
  yield setCookiePref();

  let test_uri = "http://mochi.test:8888/browser/dom/security/test/cors/file_cors_logging_test.html";

  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");

  let toolbox = yield openToolboxForTab(tab, "webconsole");
  ok(toolbox, "Got toolbox");
  let hud = toolbox.getCurrentPanel().hud;
  ok(hud, "Got hud");

  if (!webconsole) {
    registerCleanupFunction(do_cleanup);
    hud.ui.on("new-messages", on_new_message);
    webconsole = hud;
  }

  BrowserTestUtils.loadURI(gBrowser, test_uri);

  yield BrowserTestUtils.waitForLocationChange(gBrowser, test_uri+"#finished");

  // Different OS combinations
  ok(messages_seen > 0, "Saw " + messages_seen + " messages.");

  yield BrowserTestUtils.removeTab(tab);
});
