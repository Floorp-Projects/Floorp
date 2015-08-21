/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Test for Bug 1182551 -
 *
 * This test has a top level HTTP page with an HTTPS iframe.  The HTTPS iframe
 * includes an HTTP image.  We check that the top level security state is
 * STATE_IS_INSECURE.  The mixed content from the iframe shouldn't "upgrade"
 * the HTTP top level page to broken HTTPS.
 */

const gHttpTestRoot = "http://example.com/browser/browser/base/content/test/general/";

let gTestBrowser = null;

function SecStateTestsCompleted() {
  gBrowser.removeCurrentTab();
  window.focus();
  finish();
}

function test() {
  waitForExplicitFinish();
  SpecialPowers.pushPrefEnv({"set": [
    ["security.mixed_content.block_active_content", true],
    ["security.mixed_content.block_display_content", false]
  ]}, SecStateTests);
}

function SecStateTests() {
  let url = gHttpTestRoot + "file_mixedContentFramesOnHttp.html";
  gBrowser.selectedTab = gBrowser.addTab();
  gTestBrowser = gBrowser.selectedBrowser;
  whenLoaded(gTestBrowser, SecStateTest1);
  gTestBrowser.contentWindow.location = url;
}

// The http page loads an https frame with an http image.
function SecStateTest1() {
  // check security state is insecure
  isSecurityState("insecure");
  assertMixedContentBlockingState(gTestBrowser, {activeLoaded: false, activeBlocked: false, passiveLoaded: true});

  SecStateTestsCompleted();
}

function whenLoaded(aElement, aCallback) {
  aElement.addEventListener("load", function onLoad() {
    aElement.removeEventListener("load", onLoad, true);
    executeSoon(aCallback);
  }, true);
}
