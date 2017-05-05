"use strict";

const {TabStateFlusher} = Cu.import("resource:///modules/sessionstore/TabStateFlusher.jsm", {});

add_task(function*() {
  let uri = "http://example.com/browser/browser/base/content/test/general/dummy_page.html";

  // Clear out the closed windows set to start
  while (SessionStore.getClosedWindowCount() > 0)
    SessionStore.forgetClosedWindow(0);

  let tab = gBrowser.addTab();
  tab.linkedBrowser.loadURI(uri);
  yield BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  yield TabStateFlusher.flush(tab.linkedBrowser);

  let key = tab.linkedBrowser.permanentKey;
  let win = gBrowser.replaceTabWithWindow(tab);
  yield new Promise(resolve => whenDelayedStartupFinished(win, resolve));

  is(win.gBrowser.selectedBrowser.permanentKey, key, "Should have properly copied the permanentKey");
  yield BrowserTestUtils.closeWindow(win);

  is(SessionStore.getClosedWindowCount(), 1, "Should have restore data for the closed window");

  win = SessionStore.undoCloseWindow(0);
  yield BrowserTestUtils.waitForEvent(win, "load");
  yield BrowserTestUtils.waitForEvent(win.gBrowser.tabContainer, "SSTabRestored");

  is(win.gBrowser.tabs.length, 1, "Should have restored one tab");
  is(win.gBrowser.selectedBrowser.currentURI.spec, uri, "Should have restored the right page");

  yield promiseWindowClosed(win);
});
