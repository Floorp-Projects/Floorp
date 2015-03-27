/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Tests for Bug 947079 - Fix bug in nsSecureBrowserUIImpl that sets the wrong
 * security state on a page because of a subresource load that is not on the
 * same page.
 */

// We use different domains for each test and for navigation within each test
const gHttpTestRoot1 = "http://example.com/browser/browser/base/content/test/general/";
const gHttpsTestRoot1 = "https://test1.example.com/browser/browser/base/content/test/general/";
const gHttpTestRoot2 = "http://example.net/browser/browser/base/content/test/general/";
const gHttpsTestRoot2 = "https://test2.example.com/browser/browser/base/content/test/general/";

let gTestBrowser = null;

function SecStateTestsCompleted() {
  gBrowser.removeCurrentTab();
  window.focus();
  finish();
}

function test() {
  waitForExplicitFinish();
  SpecialPowers.pushPrefEnv({"set": [["security.mixed_content.block_active_content", true],
                            ["security.mixed_content.block_display_content", false]]}, SecStateTests);
}

function SecStateTests() {
  gBrowser.selectedTab = gBrowser.addTab();
  gTestBrowser = gBrowser.selectedBrowser;

  whenLoaded(gTestBrowser, SecStateTest1A);
  let url = gHttpTestRoot1 + "file_mixedContentFromOnunload.html";
  gTestBrowser.contentWindow.location = url;
}

// Navigation from an http page to a https page with no mixed content
// The http page loads an http image on unload
function SecStateTest1A() {
  whenLoaded(gTestBrowser, SecStateTest1B);
  let url = gHttpsTestRoot1 + "file_mixedContentFromOnunload_test1.html";
  gTestBrowser.contentWindow.location = url;
}

function SecStateTest1B() {
  // check security state.  Since current url is https and doesn't have any
  // mixed content resources, we expect it to be secure.
  isSecurityState("secure");

  whenLoaded(gTestBrowser, SecStateTest2A);

  // change locations and proceed with the second test
  let url = gHttpTestRoot2 + "file_mixedContentFromOnunload.html";
  gTestBrowser.contentWindow.location = url;
}

// Navigation from an http page to a https page that has mixed display content
// The http page loads an http image on unload
function SecStateTest2A() {
  whenLoaded(gTestBrowser, SecStateTest2B);
  let url = gHttpsTestRoot2 + "file_mixedContentFromOnunload_test2.html";
  gTestBrowser.contentWindow.location = url;
}

function SecStateTest2B() {
  isSecurityState("broken");

  SecStateTestsCompleted();
}

// Compares the security state of the page with what is expected
function isSecurityState(expectedState) {
  let ui = gTestBrowser.securityUI;
  if (!ui) {
    ok(false, "No security UI to get the security state");
    return;
  }

  const wpl = Components.interfaces.nsIWebProgressListener;

  // determine the security state
  let isSecure = ui.state & wpl.STATE_IS_SECURE;
  let isBroken = ui.state & wpl.STATE_IS_BROKEN;
  let isInsecure = ui.state & wpl.STATE_IS_INSECURE;

  let actualState;
  if (isSecure && !(isBroken || isInsecure)) {
    actualState = "secure";
  } else if (isBroken && !(isSecure || isInsecure)) {
    actualState = "broken";
  } else if (isInsecure && !(isSecure || isBroken)) {
    actualState = "insecure";
  } else {
    actualState = "unknown";
  }

  is(expectedState, actualState, "Expected state " + expectedState + " and the actual state is " + actualState + ".");
}

function whenLoaded(aElement, aCallback) {
  aElement.addEventListener("load", function onLoad() {
    aElement.removeEventListener("load", onLoad, true);
    executeSoon(aCallback);
  }, true);
}
