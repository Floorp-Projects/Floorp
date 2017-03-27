/* -*- Mode: JavaScript; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test for bug 1303838.
 * Load a tab with some links, emulate link clicks and check if the
 * browser would switch to the existing target tab opened by previous
 * link click if loadDivertedInBackground is set to true.
 */

"use strict";

const BASE_URL = "http://mochi.test:8888/browser/dom/base/test/";

add_task(function*() {
  yield* testLinkClick(false, false);
  yield* testLinkClick(false, true);
  yield* testLinkClick(true, false);
  yield* testLinkClick(true, true);
});

function* testLinkClick(withFrame, loadDivertedInBackground) {
  yield SpecialPowers.pushPrefEnv({"set": [["browser.tabs.loadDivertedInBackground", loadDivertedInBackground]]});

  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser,
    BASE_URL + (withFrame ? "file_bug1303838_with_iframe.html" : "file_bug1303838.html"));
  is(gBrowser.tabs.length, 2, "check tabs.length");
  is(gBrowser.selectedTab, tab, "check selectedTab");

  info("Test normal links with loadDivertedInBackground=" + loadDivertedInBackground + ", withFrame=" + withFrame);

  let [testTab] = yield clickLink(withFrame, "#link-1", tab.linkedBrowser);
  is(gBrowser.tabs.length, 3, "check tabs.length");
  is(gBrowser.selectedTab, loadDivertedInBackground ? tab : testTab, "check selectedTab");

  if (!loadDivertedInBackground) {
    yield BrowserTestUtils.switchTab(gBrowser, tab);
  }
  yield clickLink(withFrame, "#link-2", tab.linkedBrowser, testTab.linkedBrowser, !loadDivertedInBackground);
  is(gBrowser.tabs.length, 3, "check tabs.length");
  is(gBrowser.selectedTab, loadDivertedInBackground ? tab : testTab, "check selectedTab");

  if (!loadDivertedInBackground) {
    yield BrowserTestUtils.switchTab(gBrowser, tab);
  }
  yield clickLink(withFrame, "#link-3", tab.linkedBrowser, testTab.linkedBrowser, !loadDivertedInBackground);
  is(gBrowser.tabs.length, 3, "check tabs.length");
  is(gBrowser.selectedTab, loadDivertedInBackground ? tab : testTab, "check selectedTab");

  if (!loadDivertedInBackground) {
    yield BrowserTestUtils.switchTab(gBrowser, tab);
  }
  yield clickLink(withFrame, "#link-4", tab.linkedBrowser, testTab.linkedBrowser, !loadDivertedInBackground, 2);
  is(gBrowser.tabs.length, 3, "check tabs.length");
  is(gBrowser.selectedTab, loadDivertedInBackground ? tab : testTab, "check selectedTab");

  info("Test anchor links with loadDivertedInBackground=" + loadDivertedInBackground + ", withFrame=" + withFrame);

  if (!loadDivertedInBackground) {
    yield BrowserTestUtils.switchTab(gBrowser, tab);
  }
  yield clickLink(withFrame, "#anchor-link-1", tab.linkedBrowser, testTab.linkedBrowser, !loadDivertedInBackground);
  is(gBrowser.tabs.length, 3, "check tabs.length");
  is(gBrowser.selectedTab, loadDivertedInBackground ? tab : testTab, "check selectedTab");

  if (!loadDivertedInBackground) {
    yield BrowserTestUtils.switchTab(gBrowser, tab);
  }
  yield clickLink(withFrame, "#anchor-link-2", tab.linkedBrowser, testTab.linkedBrowser, !loadDivertedInBackground);
  is(gBrowser.tabs.length, 3, "check tabs.length");
  is(gBrowser.selectedTab, loadDivertedInBackground ? tab : testTab, "check selectedTab");

  if (!loadDivertedInBackground) {
    yield BrowserTestUtils.switchTab(gBrowser, tab);
  }
  yield clickLink(withFrame, "#anchor-link-3", tab.linkedBrowser, testTab.linkedBrowser, !loadDivertedInBackground);
  is(gBrowser.tabs.length, 3, "check tabs.length");
  is(gBrowser.selectedTab, loadDivertedInBackground ? tab : testTab, "check selectedTab");

  info("Test iframe links with loadDivertedInBackground=" + loadDivertedInBackground + ", withFrame=" + withFrame);

  if (!loadDivertedInBackground) {
    yield BrowserTestUtils.switchTab(gBrowser, tab);
  }
  yield clickLink(withFrame, "#frame-link-1", tab.linkedBrowser, testTab.linkedBrowser, !loadDivertedInBackground);
  is(gBrowser.tabs.length, 3, "check tabs.length");
  is(gBrowser.selectedTab, loadDivertedInBackground ? tab : testTab, "check selectedTab");

  if (!loadDivertedInBackground) {
    yield BrowserTestUtils.switchTab(gBrowser, tab);
  }
  yield clickLink(withFrame, "#frame-link-2", tab.linkedBrowser, testTab.linkedBrowser, !loadDivertedInBackground);
  is(gBrowser.tabs.length, 3, "check tabs.length");
  is(gBrowser.selectedTab, loadDivertedInBackground ? tab : testTab, "check selectedTab");

  if (!loadDivertedInBackground) {
    yield BrowserTestUtils.switchTab(gBrowser, tab);
  }
  yield clickLink(withFrame, "#frame-link-3", tab.linkedBrowser, testTab.linkedBrowser, !loadDivertedInBackground);
  is(gBrowser.tabs.length, 3, "check tabs.length");
  is(gBrowser.selectedTab, loadDivertedInBackground ? tab : testTab, "check selectedTab");

  yield BrowserTestUtils.removeTab(testTab);
  yield BrowserTestUtils.removeTab(tab);
}

function clickLink(isFrame, linkId, browser, testBrowser, awaitTabSwitch = false, locationChangeNum = 1) {
  let promises = [];
  if (awaitTabSwitch) {
    promises.push(waitForTabSwitch(gBrowser));
  }
  promises.push(testBrowser ?
                waitForLocationChange(testBrowser, locationChangeNum) :
                BrowserTestUtils.waitForNewTab(gBrowser));
  promises.push(ContentTask.spawn(browser, [isFrame, linkId],
                                  ([contentIsFrame, contentLinkId]) => {
    let doc = content.document;
    if (contentIsFrame) {
      let frame = content.document.getElementById("frame");
      doc = frame.contentDocument;
    }
    info("Clicking " + contentLinkId);
    doc.querySelector(contentLinkId).click();
  }));
  return Promise.all(promises);
}

function waitForTabSwitch(tabbrowser) {
  info("Waiting for TabSwitch");
  return new Promise(resolve => {
    tabbrowser.addEventListener("TabSwitchDone", function onSwitch() {
      info("TabSwitch done");
      tabbrowser.removeEventListener("TabSwitchDone", onSwitch);
      resolve(tabbrowser.selectedTab);
    });
  });
}

// We need a longer lifetime reference to ensure the listener is alive when
// location change occurs.
let locationChangeListener;
function waitForLocationChange(browser, locationChangeNum) {
  info("Waiting for " + locationChangeNum + " LocationChange");
  return new Promise(resolve => {
    let seen = 0;
    locationChangeListener = {
      onLocationChange(aWebProgress, aRequest, aLocation, aFlags) {
        info("LocationChange: " + aLocation.spec);
        if (++seen == locationChangeNum) {
          browser.removeProgressListener(this);
          resolve();
        }
      },
      QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                             Ci.nsISupportsWeakReference])
    };
    browser.addProgressListener(locationChangeListener);
  });
}
