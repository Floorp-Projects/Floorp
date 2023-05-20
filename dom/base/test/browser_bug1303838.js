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

add_task(async function () {
  // On Linux, in our test automation, the mouse cursor floats over
  // the first tab, which causes it to be warmed up when tab warming
  // is enabled. The TabSwitchDone event doesn't fire until the warmed
  // tab is evicted, which is after a few seconds. That means that
  // this test ends up taking longer than we'd like, since its waiting
  // for the TabSwitchDone event between tab switches.
  //
  // So now we make sure that warmed tabs are evicted very shortly
  // after warming to avoid the test running too long.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.tabs.remote.warmup.unloadDelayMs", 50]],
  });
  await testLinkClick(false, false);
  await testLinkClick(false, true);
  await testLinkClick(true, false);
  await testLinkClick(true, true);
});

async function waitForTestReady(loadDivertedInBackground, tab) {
  if (!loadDivertedInBackground) {
    await BrowserTestUtils.switchTab(gBrowser, tab);
  } else {
    await new Promise(resolve => setTimeout(resolve, 0));
  }
}

async function testLinkClick(withFrame, loadDivertedInBackground) {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.tabs.loadDivertedInBackground", loadDivertedInBackground]],
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    BASE_URL +
      (withFrame ? "file_bug1303838_with_iframe.html" : "file_bug1303838.html")
  );
  is(gBrowser.tabs.length, 2, "check tabs.length");
  is(gBrowser.selectedTab, tab, "check selectedTab");

  info(
    "Test normal links with loadDivertedInBackground=" +
      loadDivertedInBackground +
      ", withFrame=" +
      withFrame
  );

  let testTabPromise = BrowserTestUtils.waitForNewTab(gBrowser);
  await clickLink(withFrame, "#link-1", tab.linkedBrowser);
  let testTab = await testTabPromise;
  is(gBrowser.tabs.length, 3, "check tabs.length");
  is(
    gBrowser.selectedTab,
    loadDivertedInBackground ? tab : testTab,
    "check selectedTab"
  );

  await waitForTestReady(loadDivertedInBackground, tab);
  await clickLink(
    withFrame,
    "#link-2",
    tab.linkedBrowser,
    testTab.linkedBrowser,
    !loadDivertedInBackground
  );
  is(gBrowser.tabs.length, 3, "check tabs.length");
  is(
    gBrowser.selectedTab,
    loadDivertedInBackground ? tab : testTab,
    "check selectedTab"
  );

  await waitForTestReady(loadDivertedInBackground, tab);
  await clickLink(
    withFrame,
    "#link-3",
    tab.linkedBrowser,
    testTab.linkedBrowser,
    !loadDivertedInBackground
  );
  is(gBrowser.tabs.length, 3, "check tabs.length");
  is(
    gBrowser.selectedTab,
    loadDivertedInBackground ? tab : testTab,
    "check selectedTab"
  );

  await waitForTestReady(loadDivertedInBackground, tab);
  await clickLink(
    withFrame,
    "#link-4",
    tab.linkedBrowser,
    testTab.linkedBrowser,
    !loadDivertedInBackground
  );
  is(gBrowser.tabs.length, 3, "check tabs.length");
  is(
    gBrowser.selectedTab,
    loadDivertedInBackground ? tab : testTab,
    "check selectedTab"
  );

  // Location APIs shouldn't steal focus.
  await waitForTestReady(loadDivertedInBackground, tab);
  await clickLink(
    withFrame,
    "#link-5",
    tab.linkedBrowser,
    testTab.linkedBrowser,
    /* awaitTabSwitch = */ false
  );
  is(gBrowser.tabs.length, 3, "check tabs.length");
  is(gBrowser.selectedTab, tab, "check selectedTab");

  await waitForTestReady(/* diverted = */ true, tab);
  await clickLink(
    withFrame,
    "#link-6",
    tab.linkedBrowser,
    testTab.linkedBrowser,
    /* awaitTabSwitch = */ false
  );
  is(gBrowser.tabs.length, 3, "check tabs.length");
  is(gBrowser.selectedTab, tab, "check selectedTab");

  await waitForTestReady(/* diverted = */ true, tab);
  let loaded = BrowserTestUtils.browserLoaded(
    testTab.linkedBrowser,
    true,
    "data:text/html;charset=utf-8,testFrame"
  );
  await clickLink(
    withFrame,
    "#link-7",
    tab.linkedBrowser,
    testTab.linkedBrowser,
    !loadDivertedInBackground,
    false
  );
  await loaded;
  is(gBrowser.tabs.length, 3, "check tabs.length");
  is(
    gBrowser.selectedTab,
    loadDivertedInBackground ? tab : testTab,
    "check selectedTab"
  );

  info(
    "Test anchor links with loadDivertedInBackground=" +
      loadDivertedInBackground +
      ", withFrame=" +
      withFrame
  );

  await waitForTestReady(loadDivertedInBackground, tab);
  await clickLink(
    withFrame,
    "#anchor-link-1",
    tab.linkedBrowser,
    testTab.linkedBrowser,
    !loadDivertedInBackground
  );
  is(gBrowser.tabs.length, 3, "check tabs.length");
  is(
    gBrowser.selectedTab,
    loadDivertedInBackground ? tab : testTab,
    "check selectedTab"
  );

  await waitForTestReady(loadDivertedInBackground, tab);
  await clickLink(
    withFrame,
    "#anchor-link-2",
    tab.linkedBrowser,
    testTab.linkedBrowser,
    !loadDivertedInBackground
  );
  is(gBrowser.tabs.length, 3, "check tabs.length");
  is(
    gBrowser.selectedTab,
    loadDivertedInBackground ? tab : testTab,
    "check selectedTab"
  );

  await waitForTestReady(loadDivertedInBackground, tab);
  await clickLink(
    withFrame,
    "#anchor-link-3",
    tab.linkedBrowser,
    testTab.linkedBrowser,
    !loadDivertedInBackground
  );
  is(gBrowser.tabs.length, 3, "check tabs.length");
  is(
    gBrowser.selectedTab,
    loadDivertedInBackground ? tab : testTab,
    "check selectedTab"
  );

  info(
    "Test iframe links with loadDivertedInBackground=" +
      loadDivertedInBackground +
      ", withFrame=" +
      withFrame
  );

  await waitForTestReady(loadDivertedInBackground, tab);
  await clickLink(
    withFrame,
    "#frame-link-1",
    tab.linkedBrowser,
    testTab.linkedBrowser,
    !loadDivertedInBackground,
    true
  );
  is(gBrowser.tabs.length, 3, "check tabs.length");
  is(
    gBrowser.selectedTab,
    loadDivertedInBackground ? tab : testTab,
    "check selectedTab"
  );

  await waitForTestReady(loadDivertedInBackground, tab);
  await clickLink(
    withFrame,
    "#frame-link-2",
    tab.linkedBrowser,
    testTab.linkedBrowser,
    !loadDivertedInBackground,
    true
  );
  is(gBrowser.tabs.length, 3, "check tabs.length");
  is(
    gBrowser.selectedTab,
    loadDivertedInBackground ? tab : testTab,
    "check selectedTab"
  );

  await waitForTestReady(loadDivertedInBackground, tab);
  await clickLink(
    withFrame,
    "#frame-link-3",
    tab.linkedBrowser,
    testTab.linkedBrowser,
    !loadDivertedInBackground,
    true
  );
  is(gBrowser.tabs.length, 3, "check tabs.length");
  is(
    gBrowser.selectedTab,
    loadDivertedInBackground ? tab : testTab,
    "check selectedTab"
  );

  BrowserTestUtils.removeTab(testTab);
  BrowserTestUtils.removeTab(tab);
}

function clickLink(
  isFrame,
  linkId,
  browser,
  testBrowser,
  awaitTabSwitch = false,
  targetsFrame = false,
  locationChangeNum = 1
) {
  let promises = [];
  if (awaitTabSwitch) {
    promises.push(waitForTabSwitch(gBrowser));
  }
  if (testBrowser) {
    promises.push(
      waitForLocationChange(targetsFrame, testBrowser, locationChangeNum)
    );
  }

  info("BC children: " + browser.browsingContext.children.length);
  promises.push(
    BrowserTestUtils.synthesizeMouseAtCenter(
      linkId,
      {},
      isFrame ? browser.browsingContext.children[0] : browser
    )
  );
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

let locationChangeListener;
function waitForLocationChange(isFrame, browser, locationChangeNum) {
  if (isFrame) {
    return waitForFrameLocationChange(browser, locationChangeNum);
  }

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
      QueryInterface: ChromeUtils.generateQI([
        "nsIWebProgressListener",
        "nsISupportsWeakReference",
      ]),
    };
    browser.addProgressListener(locationChangeListener);
  });
}

function waitForFrameLocationChange(browser, locationChangeNum) {
  info("Waiting for " + locationChangeNum + " LocationChange in subframe");
  return SpecialPowers.spawn(browser, [locationChangeNum], async changeNum => {
    let seen = 0;
    let webprogress = content.docShell.QueryInterface(Ci.nsIWebProgress);
    await new Promise(resolve => {
      locationChangeListener = {
        onLocationChange(aWebProgress, aRequest, aLocation, aFlags) {
          info("LocationChange: " + aLocation.spec);
          if (++seen == changeNum) {
            resolve();
          }
        },
        QueryInterface: ChromeUtils.generateQI([
          "nsIWebProgressListener",
          "nsISupportsWeakReference",
        ]),
      };
      webprogress.addProgressListener(
        locationChangeListener,
        Ci.nsIWebProgress.NOTIFY_LOCATION
      );
    });
    webprogress.removeProgressListener(locationChangeListener);
  });
}
