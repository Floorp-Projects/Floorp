/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { TabState } = ChromeUtils.import(
  "resource:///modules/sessionstore/TabState.jsm"
);

add_task(async function() {
  TEST_LOGIN1 = await addLogin(TEST_LOGIN1);
  const testGuid = TEST_LOGIN1.guid;
  const tab = BrowserTestUtils.addTab(gBrowser, "about:logins");
  const browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  await ContentTask.spawn(browser, testGuid, async function(guid) {
    let loginList = Cu.waiveXrays(content.document.querySelector("login-list"));
    let loginFound = await ContentTaskUtils.waitForCondition(() => {
      return (
        loginList._loginGuidsSortedOrder.length == 1 &&
        loginList._loginGuidsSortedOrder[0] == guid
      );
    }, "Waiting for login to be displayed in page");
    ok(loginFound, "Confirming that login is displayed in page");
  });

  BrowserTestUtils.removeTab(tab);
  info("Adding a lazy about:logins tab...");
  let lazyTab = BrowserTestUtils.addTab(gBrowser, "about:logins", {
    createLazyBrowser: true,
  });

  is(lazyTab.linkedPanel, "", "Tab is lazy");
  let tabLoaded = new Promise(resolve => {
    gBrowser.addTabsProgressListener({
      async onLocationChange(aBrowser) {
        if (lazyTab.linkedBrowser == aBrowser) {
          gBrowser.removeTabsProgressListener(this);
          await Promise.resolve();
          resolve();
        }
      },
    });
  });

  info("Switching tab to cause it to get restored");
  await BrowserTestUtils.switchTab(gBrowser, lazyTab);

  await tabLoaded;

  let lazyBrowser = lazyTab.linkedBrowser;
  await ContentTask.spawn(lazyBrowser, testGuid, async function(guid) {
    let loginList = Cu.waiveXrays(content.document.querySelector("login-list"));
    let loginFound = await ContentTaskUtils.waitForCondition(() => {
      return (
        loginList._loginGuidsSortedOrder.length == 1 &&
        loginList._loginGuidsSortedOrder[0] == guid
      );
    }, "Waiting for login to be displayed in restored page");
    ok(loginFound, "Confirming that login is displayed in restored page");
  });

  BrowserTestUtils.removeTab(lazyTab);
});
