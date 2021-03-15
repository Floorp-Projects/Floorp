/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const URL = "http://mochi.test:8888/browser/";
const PREF = "browser.sessionstore.restore_on_demand";

add_task(async () => {
  Services.prefs.setBoolPref(PREF, true);
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref(PREF);
  });

  let tab = await preparePendingTab();

  let deferredTab = PromiseUtils.defer();

  let win = gBrowser.replaceTabWithWindow(tab);
  win.addEventListener(
    "before-initial-tab-adopted",
    async () => {
      let [newTab] = win.gBrowser.tabs;
      await BrowserTestUtils.browserLoaded(newTab.linkedBrowser);
      deferredTab.resolve(newTab);
    },
    { once: true }
  );

  let newTab = await deferredTab.promise;
  is(newTab.linkedBrowser.currentURI.spec, URL, "correct url should be loaded");
  ok(!newTab.hasAttribute("pending"), "tab should not be pending");

  win.close();
});

async function preparePendingTab(aCallback) {
  let tab = BrowserTestUtils.addTab(gBrowser, URL);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  let sessionUpdatePromise = BrowserTestUtils.waitForSessionStoreUpdate(tab);
  BrowserTestUtils.removeTab(tab);
  await sessionUpdatePromise;

  let [{ state }] = JSON.parse(SessionStore.getClosedTabData(window));

  tab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  SessionStore.setTabState(tab, JSON.stringify(state));
  ok(tab.hasAttribute("pending"), "tab should be pending");

  return tab;
}
