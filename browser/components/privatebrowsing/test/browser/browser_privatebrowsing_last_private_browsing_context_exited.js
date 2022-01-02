/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_no_notification_when_pb_autostart() {
  let observedLastPBContext = false;
  let observerExited = {
    observe(aSubject, aTopic, aData) {
      observedLastPBContext = true;
    },
  };
  Services.obs.addObserver(observerExited, "last-pb-context-exited");

  await SpecialPowers.pushPrefEnv({
    set: [["browser.privatebrowsing.autostart", true]],
  });

  let win = await BrowserTestUtils.openNewBrowserWindow();

  let browser = win.gBrowser.selectedTab.linkedBrowser;
  ok(browser.browsingContext.usePrivateBrowsing, "should use private browsing");

  await BrowserTestUtils.closeWindow(win);

  await SpecialPowers.popPrefEnv();
  Services.obs.removeObserver(observerExited, "last-pb-context-exited");
  ok(!observedLastPBContext, "No last-pb-context-exited notification seen");
});

add_task(async function test_notification_when_about_preferences() {
  let observedLastPBContext = false;
  let observerExited = {
    observe(aSubject, aTopic, aData) {
      observedLastPBContext = true;
    },
  };
  Services.obs.addObserver(observerExited, "last-pb-context-exited");

  let win = await BrowserTestUtils.openNewBrowserWindow({ private: true });

  let browser = win.gBrowser.selectedTab.linkedBrowser;
  ok(browser.browsingContext.usePrivateBrowsing, "should use private browsing");
  ok(browser.browsingContext.isContent, "should be content browsing context");

  let tab = await BrowserTestUtils.addTab(win.gBrowser, "about:preferences");
  ok(
    tab.linkedBrowser.browsingContext.usePrivateBrowsing,
    "should use private browsing"
  );
  ok(
    tab.linkedBrowser.browsingContext.isContent,
    "should be content browsing context"
  );

  let tabClose = BrowserTestUtils.waitForTabClosing(win.gBrowser.selectedTab);
  BrowserTestUtils.removeTab(win.gBrowser.selectedTab);
  await tabClose;

  ok(!observedLastPBContext, "No last-pb-context-exited notification seen");

  await BrowserTestUtils.closeWindow(win);

  Services.obs.removeObserver(observerExited, "last-pb-context-exited");
  ok(observedLastPBContext, "No last-pb-context-exited notification seen");
});
