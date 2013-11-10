/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  Task.spawn(function task() {
    let tab = gBrowser.selectedTab = gBrowser.addTab("about:mozilla");
    yield promiseBrowserLoaded(gBrowser.selectedBrowser);

    let win = gBrowser.replaceTabWithWindow(tab);
    yield promiseDelayedStartupFinished(win);
    yield promiseBrowserHasURL(win.gBrowser.browsers[0], "about:mozilla");

    win.duplicateTabIn(win.gBrowser.selectedTab, "tab");
    let browser = win.gBrowser.browsers[1];
    yield promiseBrowserLoaded(browser);
    is(browser.currentURI.spec, "about:mozilla", "tab was duplicated");

    win.close();
  }).then(finish);
}

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
