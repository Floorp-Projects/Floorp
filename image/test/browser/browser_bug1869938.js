/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This test opens a private browsing window, then opens a content page in it
 * that sets a favicon that is a data uri containing a svg file, which
 * contains an image element that refers to a data uri containing a png image.
 * This tests that we don't hit an assert when loading the png in the favicon
 * in the parent process with this setup.
 */

function waitForLinkAvailable(browser, win) {
  let resolve, reject;

  let listener = {
    onLinkIconAvailable(b, dataURI, iconURI) {
      // Ignore icons for other browsers or empty icons.
      if (browser !== b || !iconURI) {
        return;
      }

      win.gBrowser.removeTabsProgressListener(listener);
      resolve(iconURI);
    },
  };

  let promise = new Promise((res, rej) => {
    resolve = res;
    reject = rej;

    win.gBrowser.addTabsProgressListener(listener);
  });

  promise.cancel = () => {
    win.gBrowser.removeTabsProgressListener(listener);

    reject();
  };

  return promise;
}

add_task(async function test() {
  function httpURL(filename) {
    let chromeURL = getRootDirectory(gTestPath) + filename;
    return chromeURL.replace(
      "chrome://mochitests/content/",
      "http://mochi.test:8888/"
    );
  }

  let win = await BrowserTestUtils.openNewBrowserWindow({ private: true });

  const pageUrl = httpURL("helper1869938.html");

  let tab = (win.gBrowser.selectedTab = BrowserTestUtils.addTab(
    win.gBrowser,
    "about:blank"
  ));

  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  let faviconPromise = waitForLinkAvailable(tab.linkedBrowser, win);

  BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, pageUrl);

  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  await new Promise(resolve => {
    waitForFocus(resolve, win);
  });

  await faviconPromise;

  // do a couple rafs here to ensure its loaded and displayed
  await new Promise(r => requestAnimationFrame(r));
  await new Promise(r => requestAnimationFrame(r));

  await BrowserTestUtils.closeWindow(win);

  win = null;
  tab = null;
  faviconPromise = null;

  ok(true, "we got here and didn't crash/assert");
});
