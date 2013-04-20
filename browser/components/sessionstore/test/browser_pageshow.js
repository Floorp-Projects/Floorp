/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  TestRunner.run();
}

/**
 * This test ensures that loading a page from bfcache (by going back or forward
 * in history) marks the window as dirty and causes data about the tab that
 * changed to be re-collected.
 *
 * We will do this by creating a tab with two history entries and going back
 * to the first. When we now request the current browser state from the
 * session store service the first history entry must be selected.
 */

function runTests() {
  // Create a dummy window that is regarded as active. We need to do this
  // because we always collect data for tabs of active windows no matter if
  // the window is dirty or not.
  let win = OpenBrowserWindow();
  yield waitForLoad(win);

  // Create a tab with two history entries.
  let tab = gBrowser.selectedTab = gBrowser.addTab("about:blank");
  yield loadURI("about:robots");
  yield loadURI("about:mozilla");

  // All windows currently marked as dirty will be written to disk
  // and thus marked clean afterwards.
  yield forceWriteState();

  // Go back to 'about:robots' - which is loaded from the bfcache and thus
  // will not fire a 'load' event but a 'pageshow' event with persisted=true.
  waitForPageShow();
  yield gBrowser.selectedBrowser.goBack();
  is(tab.linkedBrowser.currentURI.spec, "about:robots", "url is about:robots");

  // If by receiving the 'pageshow' event the first window has correctly
  // been marked as dirty, getBrowserState() should return the tab we created
  // with the right history entry (about:robots) selected.
  let state = JSON.parse(ss.getBrowserState());
  is(state.windows[0].tabs[1].index, 1, "first history entry is selected");

  // Clean up after ourselves.
  gBrowser.removeTab(tab);
  win.close();
}

function forceWriteState() {
  const PREF = "browser.sessionstore.interval";
  const TOPIC = "sessionstore-state-write";

  Services.obs.addObserver(function observe() {
    Services.obs.removeObserver(observe, TOPIC);
    Services.prefs.clearUserPref(PREF);
    executeSoon(next);
  }, TOPIC, false);

  Services.prefs.setIntPref(PREF, 0);
}

function loadURI(aURI) {
  let browser = gBrowser.selectedBrowser;
  waitForLoad(browser);
  browser.loadURI(aURI);
}

function waitForLoad(aElement) {
  aElement.addEventListener("load", function onLoad() {
    aElement.removeEventListener("load", onLoad, true);
    executeSoon(next);
  }, true);
}

function waitForPageShow() {
  let mm = gBrowser.selectedBrowser.messageManager;

  mm.addMessageListener("SessionStore:pageshow", function onPageShow() {
    mm.removeMessageListener("SessionStore:pageshow", onPageShow);
    executeSoon(next);
  });
}
