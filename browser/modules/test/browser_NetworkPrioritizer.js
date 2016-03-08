/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/** Tests for NetworkPrioritizer.jsm (Bug 514490) **/

const LOWEST  = Ci.nsISupportsPriority.PRIORITY_LOWEST;
const LOW     = Ci.nsISupportsPriority.PRIORITY_LOW;
const NORMAL  = Ci.nsISupportsPriority.PRIORITY_NORMAL;
const HIGH    = Ci.nsISupportsPriority.PRIORITY_HIGH;
const HIGHEST = Ci.nsISupportsPriority.PRIORITY_HIGHEST;

const DELTA = NORMAL - LOW; // lower value means higher priority

// Test helper functions.
// getPriority and setPriority can take a tab or a Browser
function* getPriority(aBrowser) {
  if (aBrowser.localName == "tab")
    aBrowser = aBrowser.linkedBrowser;

  return yield ContentTask.spawn(aBrowser, null, function* () {
    return docShell.QueryInterface(Components.interfaces.nsIWebNavigation)
                   .QueryInterface(Components.interfaces.nsIDocumentLoader)
                   .loadGroup
                   .QueryInterface(Components.interfaces.nsISupportsPriority)
                   .priority;
  });
}

function* setPriority(aBrowser, aPriority) {
  if (aBrowser.localName == "tab")
    aBrowser = aBrowser.linkedBrowser;

  yield ContentTask.spawn(aBrowser, aPriority, function* (aPriority) {
    docShell.QueryInterface(Components.interfaces.nsIWebNavigation)
                                    .QueryInterface(Components.interfaces.nsIDocumentLoader)
                                    .loadGroup
                                    .QueryInterface(Ci.nsISupportsPriority)
                                    .priority = aPriority;
  });
}

function* isWindowState(aWindow, aTabPriorities) {
  let browsers = aWindow.gBrowser.browsers;
  // Make sure we have the right number of tabs & priorities
  is(browsers.length, aTabPriorities.length,
     "Window has expected number of tabs");
  // aState should be in format [ priority, priority, priority ]
  for (let i = 0; i < browsers.length; i++) {
    is(yield getPriority(browsers[i]), aTabPriorities[i],
       "Tab " + i + " had expected priority");
  }
}

function promiseWaitForFocus(aWindow) {
  return new Promise((resolve) => {
    waitForFocus(resolve, aWindow);
  });
}

add_task(function*() {
  // This is the real test. It creates multiple tabs & windows, changes focus,
  // closes windows/tabs to make sure we behave correctly.
  // This test assumes that no priorities have been adjusted and the loadgroup
  // priority starts at 0.

  // Call window "window_A" to make the test easier to follow
  let window_A = window;

  // Test 1 window, 1 tab case.
  yield isWindowState(window_A, [HIGH]);

  // Exising tab is tab_A1
  let tab_A2 = window_A.gBrowser.addTab("http://example.com");
  let tab_A3 = window_A.gBrowser.addTab("about:config");
  yield BrowserTestUtils.browserLoaded(tab_A3.linkedBrowser);

  // tab_A2 isn't focused yet
  yield isWindowState(window_A, [HIGH, NORMAL, NORMAL]);

  // focus tab_A2 & make sure priority got updated
  window_A.gBrowser.selectedTab = tab_A2;
  yield isWindowState(window_A, [NORMAL, HIGH, NORMAL]);

  window_A.gBrowser.removeTab(tab_A2);
  // Next tab is auto selected synchronously as part of removeTab, and we
  // expect the priority to be updated immediately.
  yield isWindowState(window_A, [NORMAL, HIGH]);

  // Open another window then play with focus
  let window_B = yield BrowserTestUtils.openNewBrowserWindow();

  yield promiseWaitForFocus(window_B);
  yield isWindowState(window_A, [LOW, NORMAL]);
  yield isWindowState(window_B, [HIGH]);

  yield promiseWaitForFocus(window_A);
  yield isWindowState(window_A, [NORMAL, HIGH]);
  yield isWindowState(window_B, [NORMAL]);

  yield promiseWaitForFocus(window_B);
  yield isWindowState(window_A, [LOW, NORMAL]);
  yield isWindowState(window_B, [HIGH]);

  // Cleanup
  window_A.gBrowser.removeTab(tab_A3);
  yield BrowserTestUtils.closeWindow(window_B);
});

add_task(function*() {
  // This is more a test of nsLoadGroup and how it handles priorities. But since
  // we depend on its behavior, it's good to test it. This is testing that there
  // are no errors if we adjust beyond nsISupportsPriority's bounds.

  yield promiseWaitForFocus();

  let tab1 = gBrowser.tabs[0];
  let oldPriority = yield getPriority(tab1);

  // Set the priority of tab1 to the lowest possible. Selecting the other tab
  // will try to lower it
  yield setPriority(tab1, LOWEST);

  let tab2 = gBrowser.addTab("http://example.com");
  yield BrowserTestUtils.browserLoaded(tab2.linkedBrowser);
  gBrowser.selectedTab = tab2;
  is(yield getPriority(tab1), LOWEST - DELTA, "Can adjust priority beyond 'lowest'");

  // Now set priority to "highest" and make sure that no errors occur.
  yield setPriority(tab1, HIGHEST);
  gBrowser.selectedTab = tab1;

  is(yield getPriority(tab1), HIGHEST + DELTA, "Can adjust priority beyond 'highest'");

  // Cleanup
  gBrowser.removeTab(tab2);
  yield setPriority(tab1, oldPriority);
});

add_task(function*() {
  // This tests that the priority doesn't get lost when switching the browser's remoteness

  if (!gMultiProcessBrowser) {
    return;
  }

  let browser = gBrowser.selectedBrowser;

  browser.loadURI("http://example.com");
  yield BrowserTestUtils.browserLoaded(browser);
  ok(browser.isRemoteBrowser, "web page should be loaded in remote browser");
  is(yield getPriority(browser), HIGH, "priority of selected tab should be 'high'");

  browser.loadURI("about:rights");
  yield BrowserTestUtils.browserLoaded(browser);
  ok(!browser.isRemoteBrowser, "about:rights should switch browser to non-remote");
  is(yield getPriority(browser), HIGH,
     "priority of selected tab should be 'high' when going from remote to non-remote");

  browser.loadURI("http://example.com");
  yield BrowserTestUtils.browserLoaded(browser);
  ok(browser.isRemoteBrowser, "going from about:rights to web page should switch browser to remote");
  is(yield getPriority(browser), HIGH,
     "priority of selected tab should be 'high' when going from non-remote to remote");
});
