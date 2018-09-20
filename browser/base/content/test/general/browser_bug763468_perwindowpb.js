/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// This test makes sure that opening a new tab in private browsing mode opens about:privatebrowsing
add_task(async function testPBNewTab() {
  registerCleanupFunction(async function() {
    for (let win of windowsToClose) {
      await BrowserTestUtils.closeWindow(win);
    }
  });

  let windowsToClose = [];

  async function doTest(aIsPrivateMode) {
    let newTabURL;
    let mode;
    let win = await BrowserTestUtils.openNewBrowserWindow({private: aIsPrivateMode});
    windowsToClose.push(win);

    if (aIsPrivateMode) {
      mode = "per window private browsing";
      newTabURL = "about:privatebrowsing";
    } else {
      mode = "normal";
      newTabURL = "about:newtab";
    }
    await openNewTab(win, newTabURL);

    is(win.gBrowser.currentURI.spec, newTabURL,
      "URL of NewTab should be " + newTabURL + " in " + mode + " mode");
  }

  await doTest(false);
  await doTest(true);
  await doTest(false);
});

async function openNewTab(aWindow, aExpectedURL) {
  // Open a new tab
  aWindow.BrowserOpenTab();

  let browser = aWindow.gBrowser.selectedBrowser;
  let loadPromise = BrowserTestUtils.browserLoaded(browser, false, aExpectedURL);
  let alreadyLoaded = await ContentTask.spawn(browser, aExpectedURL, url => {
    let doc = content.document;
    return doc && doc.readyState === "complete" && doc.location.href == url;
  });
  if (!alreadyLoaded) {
    await loadPromise;
  }
}
