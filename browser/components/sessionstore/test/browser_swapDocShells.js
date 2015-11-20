"use strict";

add_task(function* () {
  let tab = gBrowser.selectedTab = gBrowser.addTab("about:mozilla");
  yield promiseBrowserLoaded(gBrowser.selectedBrowser);

  let win = gBrowser.replaceTabWithWindow(tab);
  yield promiseDelayedStartupFinished(win);
  yield promiseBrowserHasURL(win.gBrowser.browsers[0], "about:mozilla");

  win.duplicateTabIn(win.gBrowser.selectedTab, "tab");
  yield promiseTabRestored(win.gBrowser.tabs[1]);

  let browser = win.gBrowser.browsers[1];
  is(browser.currentURI.spec, "about:mozilla", "tab was duplicated");

  yield BrowserTestUtils.closeWindow(win);
});

function promiseDelayedStartupFinished(win) {
  let deferred = Promise.defer();
  whenDelayedStartupFinished(win, deferred.resolve);
  return deferred.promise;
}

function promiseBrowserHasURL(browser, url) {
  let promise = Promise.resolve();

  if (browser.contentDocument.readyState === "complete" &&
      browser.currentURI.spec === url) {
    return promise;
  }

  return promise.then(() => promiseBrowserHasURL(browser, url));
}
