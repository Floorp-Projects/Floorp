/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TIPS = {
  NONE: "",
  CLEAR: "clear",
  REFRESH: "refresh",
  UPDATE_RESTART: "update_restart",
  UPDATE_ASK: "update_ask",
  UPDATE_REFRESH: "update_refresh",
  UPDATE_WEB: "update_web",
};

const SEARCH_STRINGS = {
  CLEAR: "firefox history",
  REFRESH: "firefox slow",
  UPDATE: "firefox update",
};

add_task(async function init() {
  makeProfileResettable();
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.update1.interventions", true]],
  });
});

// Tests the refresh tip.
add_task(async function refresh() {
  // Pick the tip, which should open the refresh dialog.  Click its cancel
  // button.
  await checkIntervention({
    searchString: SEARCH_STRINGS.REFRESH,
    tip: TIPS.REFRESH,
    title:
      "Restore default settings and remove old add-ons for optimal performance.",
    button: "Refresh Nightly…",
    awaitCallback() {
      return promiseAlertDialog("cancel", [
        "chrome://global/content/resetProfile.xhtml",
        "chrome://global/content/resetProfile.xul",
      ]);
    },
  });
});

// Tests the clear tip.
add_task(async function clear() {
  // Pick the tip, which should open the refresh dialog.  Click its cancel
  // button.
  await checkIntervention({
    searchString: SEARCH_STRINGS.CLEAR,
    tip: TIPS.CLEAR,
    title: "Clear your cache, cookies, history and more.",
    button: "Choose What to Clear…",
    awaitCallback() {
      return promiseAlertDialog("cancel", [
        "chrome://browser/content/sanitize.xhtml",
        "chrome://browser/content/sanitize.xul",
      ]);
    },
  });
});

// Tests the clear tip in a private window. The clear tip shouldn't appear in
// private windows.
add_task(async function clear_private() {
  let win = await BrowserTestUtils.openNewBrowserWindow({ private: true });
  // First, make sure the extension works in PBM by triggering a non-clear
  // tip.
  let result = (await awaitTip(SEARCH_STRINGS.REFRESH, win))[0];
  Assert.strictEqual(result.payload.type, TIPS.REFRESH);

  // Blur the urlbar so that the engagement is ended.
  await UrlbarTestUtils.promisePopupClose(win, () => win.gURLBar.blur());

  // Now do a search that would trigger the clear tip.
  await awaitNoTip(SEARCH_STRINGS.CLEAR, win);

  // Blur the urlbar so that the engagement is ended.
  await UrlbarTestUtils.promisePopupClose(win, () => win.gURLBar.blur());

  await BrowserTestUtils.closeWindow(win);
});

add_task(async function tipsAreEnglishOnly() {
  // Test that Interventions are working in en-US.
  let result = (await awaitTip(SEARCH_STRINGS.REFRESH, window))[0];
  Assert.strictEqual(result.payload.type, TIPS.REFRESH);
  await UrlbarTestUtils.promisePopupClose(window, () => window.gURLBar.blur());

  // We will need to fetch new engines when we switch locales.
  let searchReinit = SearchTestUtils.promiseSearchNotification(
    "reinit-complete"
  );

  const originalAvailable = Services.locale.availableLocales;
  const originalRequested = Services.locale.requestedLocales;
  Services.locale.availableLocales = ["en-US", "de"];
  Services.locale.requestedLocales = ["de"];

  registerCleanupFunction(async () => {
    let searchReinit2 = SearchTestUtils.promiseSearchNotification(
      "reinit-complete"
    );
    Services.locale.requestedLocales = originalRequested;
    Services.locale.availableLocales = originalAvailable;
    await searchReinit2;
  });

  let appLocales = Services.locale.appLocalesAsBCP47;
  Assert.equal(appLocales[0], "de");

  await searchReinit;

  // Interventions should no longer work in the new locale.
  await awaitNoTip(SEARCH_STRINGS.CLEAR, window);
  await UrlbarTestUtils.promisePopupClose(window, () => window.gURLBar.blur());
});
