/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

let gNewTabService = Cc[
  "@mozilla.org/browser/aboutnewtab-service;1"
].getService(Ci.nsIAboutNewTabService);

async function doTest(isPrivate) {
  let win = await BrowserTestUtils.openNewBrowserWindow({ private: isPrivate });
  let defaultURL = gNewTabService.newTabURL;
  let newTabURL;
  let mode;
  let testURL = "http://example.com/";
  if (isPrivate) {
    mode = "per window private browsing";
    newTabURL = "about:privatebrowsing";
  } else {
    mode = "normal";
    newTabURL = "about:newtab";
  }

  await openNewTab(win, newTabURL);
  // Check the new tab opened while in normal/private mode
  is(
    win.gBrowser.selectedBrowser.currentURI.spec,
    newTabURL,
    "URL of NewTab should be " + newTabURL + " in " + mode + " mode"
  );

  // Set the custom newtab url
  gNewTabService.newTabURL = testURL;
  is(gNewTabService.newTabURL, testURL, "Custom newtab url is set");

  // Open a newtab after setting the custom newtab url
  await openNewTab(win, testURL);
  is(
    win.gBrowser.selectedBrowser.currentURI.spec,
    testURL,
    "URL of NewTab should be the custom url"
  );

  // Clear the custom url.
  gNewTabService.resetNewTabURL();
  is(gNewTabService.newTabURL, defaultURL, "No custom newtab url is set");

  win.gBrowser.removeTab(win.gBrowser.selectedTab);
  win.gBrowser.removeTab(win.gBrowser.selectedTab);
  await BrowserTestUtils.closeWindow(win);
}

add_task(async function test_newTabService() {
  // check whether any custom new tab url has been configured
  ok(!gNewTabService.overridden, "No custom newtab url is set");

  // test normal mode
  await doTest(false);

  // test private mode
  await doTest(true);
});

async function openNewTab(aWindow, aExpectedURL) {
  // Open a new tab
  aWindow.BrowserOpenTab();

  let browser = aWindow.gBrowser.selectedBrowser;
  let loadPromise = BrowserTestUtils.browserLoaded(
    browser,
    false,
    aExpectedURL
  );
  let alreadyLoaded = await ContentTask.spawn(browser, aExpectedURL, url => {
    let doc = content.document;
    return doc && doc.readyState === "complete" && doc.location.href == url;
  });
  if (!alreadyLoaded) {
    await loadPromise;
  }
}
