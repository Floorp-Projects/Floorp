/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests the Search Tips feature, which displays a prompt to use the Urlbar on
// the newtab page and on the user's default search engine's homepage.
// Specifically, it tests that the Tips appear when they should be appearing.
// This doesn't test the max-shown-count limit because it requires restarting
// the browser.

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  AppMenuNotifications: "resource://gre/modules/AppMenuNotifications.sys.mjs",
  HttpServer: "resource://testing-common/httpd.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarProviderSearchTips:
    "resource:///modules/UrlbarProviderSearchTips.sys.mjs",
  UrlbarUtils: "resource:///modules/UrlbarUtils.sys.mjs",
});

XPCOMUtils.defineLazyServiceGetter(
  this,
  "clipboardHelper",
  "@mozilla.org/widget/clipboardhelper;1",
  "nsIClipboardHelper"
);

// These should match the same consts in UrlbarProviderSearchTips.jsm.
const MAX_SHOWN_COUNT = 4;
const LAST_UPDATE_THRESHOLD_MS = 24 * 60 * 60 * 1000;

// We test some of the bigger Google domains.
const GOOGLE_DOMAINS = [
  "www.google.com",
  "www.google.ca",
  "www.google.co.uk",
  "www.google.com.au",
  "www.google.co.nz",
];

// In order for the persist tip to appear, the scheme of the
// search engine has to be the same as the scheme of the SERP url.
// withDNSRedirect() loads an http: url while the searchform
// of the default engine uses https. To enable the search term
// to be shown, we use the Example engine because it doesn't require
// a redirect.
const SEARCH_TERM = "chocolate";
const SEARCH_SERP_URL = `https://example.com/?q=${SEARCH_TERM}`;

add_setup(async function () {
  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();

  await SpecialPowers.pushPrefEnv({
    set: [
      [
        `browser.urlbar.tipShownCount.${UrlbarProviderSearchTips.TIP_TYPE.ONBOARD}`,
        0,
      ],
      [
        `browser.urlbar.tipShownCount.${UrlbarProviderSearchTips.TIP_TYPE.PERSIST}`,
        0,
      ],
      [
        `browser.urlbar.tipShownCount.${UrlbarProviderSearchTips.TIP_TYPE.REDIRECT}`,
        0,
      ],
      // Set following prefs so tips are actually shown.
      ["browser.laterrun.bookkeeping.profileCreationTime", 0],
      ["browser.laterrun.bookkeeping.updateAppliedTime", 0],
    ],
  });

  // Remove update history and the current active update so tips are shown.
  let updateRootDir = Services.dirsvc.get("UpdRootD", Ci.nsIFile);
  let updatesFile = updateRootDir.clone();
  updatesFile.append("updates.xml");
  let activeUpdateFile = updateRootDir.clone();
  activeUpdateFile.append("active-update.xml");
  try {
    updatesFile.remove(false);
  } catch (e) {}
  try {
    activeUpdateFile.remove(false);
  } catch (e) {}

  let defaultEngine = await Services.search.getDefault();
  let defaultEngineName = defaultEngine.name;
  Assert.equal(defaultEngineName, "Google", "Default engine should be Google.");

  // Add a mock engine so we don't hit the network loading the SERP.
  await SearchTestUtils.installSearchExtension();

  registerCleanupFunction(async () => {
    await setDefaultEngine(defaultEngineName);
    resetSearchTipsProvider();
  });
});

// Picking the tip's button should cause the Urlbar to blank out and the tip to
// be not to be shown again in any session. Telemetry should be updated.
add_task(async function pickButton_onboard() {
  UrlbarProviderSearchTips.disableTipsForCurrentSession = false;
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:newtab",
    waitForLoad: false,
  });
  await checkTip(window, UrlbarProviderSearchTips.TIP_TYPE.ONBOARD, false);

  // Click the tip button.
  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  let button = result.element.row._buttons.get("0");
  await UrlbarTestUtils.promisePopupClose(window, () => {
    EventUtils.synthesizeMouseAtCenter(button, {});
  });
  gURLBar.blur();

  // Check telemetry.
  const scalars = TelemetryTestUtils.getProcessScalars("parent", true, true);
  TelemetryTestUtils.assertKeyedScalar(
    scalars,
    "urlbar.tips",
    `${UrlbarProviderSearchTips.TIP_TYPE.ONBOARD}-picked`,
    1
  );

  Assert.equal(
    UrlbarPrefs.get(
      `tipShownCount.${UrlbarProviderSearchTips.TIP_TYPE.ONBOARD}`
    ),
    MAX_SHOWN_COUNT,
    "Onboarding tips are disabled after tip button is picked."
  );
  Assert.equal(gURLBar.value, "", "The Urlbar should be empty.");
  resetSearchTipsProvider();

  BrowserTestUtils.removeTab(tab);
});

// Picking the tip's button should cause the Urlbar to blank out and the tip to
// be not to be shown again in any session. Telemetry should be updated.
add_task(async function pickButton_redirect() {
  UrlbarProviderSearchTips.disableTipsForCurrentSession = false;
  await setDefaultEngine("Google");
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    await withDNSRedirect("www.google.com", "/", async url => {
      BrowserTestUtils.startLoadingURIString(gBrowser.selectedBrowser, url);
      await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
      await checkTip(window, UrlbarProviderSearchTips.TIP_TYPE.REDIRECT, false);

      // Click the tip button.
      let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
      let button = result.element.row._buttons.get("0");
      await UrlbarTestUtils.promisePopupClose(window, () => {
        EventUtils.synthesizeMouseAtCenter(button, {});
      });
      gURLBar.blur();
    });
  });

  // Check telemetry.
  const scalars = TelemetryTestUtils.getProcessScalars("parent", true, true);
  TelemetryTestUtils.assertKeyedScalar(
    scalars,
    "urlbar.tips",
    `${UrlbarProviderSearchTips.TIP_TYPE.REDIRECT}-picked`,
    1
  );

  Assert.equal(
    UrlbarPrefs.get(
      `tipShownCount.${UrlbarProviderSearchTips.TIP_TYPE.REDIRECT}`
    ),
    MAX_SHOWN_COUNT,
    "Redirect tips are disabled after tip button is picked."
  );
  Assert.equal(gURLBar.value, "", "The Urlbar should be empty.");
  resetSearchTipsProvider();
});

// Picking the tip's button should cause the Urlbar to keep its current
// value and the tip to be not to be shown again in any session.
// Telemetry should be updated.
add_task(async function pickButton_persist() {
  UrlbarProviderSearchTips.disableTipsForCurrentSession = false;
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.showSearchTerms.featureGate", true]],
  });

  await setDefaultEngine("Example");

  await BrowserTestUtils.withNewTab("about:blank", async () => {
    let browserLoadedPromise = BrowserTestUtils.browserLoaded(
      gBrowser.selectedBrowser,
      false,
      SEARCH_SERP_URL
    );
    BrowserTestUtils.startLoadingURIString(
      gBrowser.selectedBrowser,
      SEARCH_SERP_URL
    );
    await browserLoadedPromise;
    await checkTip(window, UrlbarProviderSearchTips.TIP_TYPE.PERSIST, false);

    let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
    let button = result.element.row._buttons.get("0");

    await UrlbarTestUtils.promisePopupClose(window, () => {
      EventUtils.synthesizeMouseAtCenter(button, {});
    });
    gURLBar.blur();

    Assert.equal(
      gURLBar.value,
      SEARCH_TERM,
      "The Urlbar should keep its existing value."
    );
  });

  // Check telemetry.
  const scalars = TelemetryTestUtils.getProcessScalars("parent", true, true);
  TelemetryTestUtils.assertKeyedScalar(
    scalars,
    "urlbar.tips",
    `${UrlbarProviderSearchTips.TIP_TYPE.PERSIST}-picked`,
    1
  );

  Assert.equal(
    UrlbarPrefs.get(
      `tipShownCount.${UrlbarProviderSearchTips.TIP_TYPE.PERSIST}`
    ),
    MAX_SHOWN_COUNT,
    "Persist tips are disabled after tip button is picked."
  );
  Assert.equal(gURLBar.value, "", "The Urlbar should be empty.");
  resetSearchTipsProvider();
  await SpecialPowers.popPrefEnv();
});

// Clicking in the input while the onboard tip is showing should have the same
// effect as picking the tip.
add_task(async function clickInInput_onboard() {
  UrlbarProviderSearchTips.disableTipsForCurrentSession = false;
  await setDefaultEngine("Google");
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:newtab",
    waitForLoad: false,
  });
  await checkTip(window, UrlbarProviderSearchTips.TIP_TYPE.ONBOARD, false);

  // Click in the input.
  await UrlbarTestUtils.promisePopupClose(window, () => {
    EventUtils.synthesizeMouseAtCenter(gURLBar.textbox.parentNode, {});
  });
  gURLBar.blur();

  // Check telemetry.
  const scalars = TelemetryTestUtils.getProcessScalars("parent", true, true);
  TelemetryTestUtils.assertKeyedScalar(
    scalars,
    "urlbar.tips",
    `${UrlbarProviderSearchTips.TIP_TYPE.ONBOARD}-picked`,
    1
  );

  Assert.equal(
    UrlbarPrefs.get(
      `tipShownCount.${UrlbarProviderSearchTips.TIP_TYPE.ONBOARD}`
    ),
    MAX_SHOWN_COUNT,
    "Onboarding tips are disabled after tip button is picked."
  );
  Assert.equal(gURLBar.value, "", "The Urlbar should be empty.");
  resetSearchTipsProvider();
  BrowserTestUtils.removeTab(tab);
});

// Pressing Ctrl+L (the open location command) while the onboard tip is showing
// should have the same effect as picking the tip.
add_task(async function openLocation_onboard() {
  UrlbarProviderSearchTips.disableTipsForCurrentSession = false;
  await setDefaultEngine("Google");
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:newtab",
    waitForLoad: false,
  });
  await checkTip(window, UrlbarProviderSearchTips.TIP_TYPE.ONBOARD, false);

  // Trigger the open location command.
  await UrlbarTestUtils.promisePopupClose(window, () => {
    document.getElementById("Browser:OpenLocation").doCommand();
  });
  gURLBar.blur();

  // Check telemetry.
  const scalars = TelemetryTestUtils.getProcessScalars("parent", true, true);
  TelemetryTestUtils.assertKeyedScalar(
    scalars,
    "urlbar.tips",
    `${UrlbarProviderSearchTips.TIP_TYPE.ONBOARD}-picked`,
    1
  );

  Assert.equal(
    UrlbarPrefs.get(
      `tipShownCount.${UrlbarProviderSearchTips.TIP_TYPE.ONBOARD}`
    ),
    MAX_SHOWN_COUNT,
    "Onboarding tips are disabled after tip button is picked."
  );
  Assert.equal(gURLBar.value, "", "The Urlbar should be empty.");
  resetSearchTipsProvider();
  BrowserTestUtils.removeTab(tab);
});

// Clicking in the input while the redirect tip is showing should have the same
// effect as picking the tip.
add_task(async function clickInInput_redirect() {
  UrlbarProviderSearchTips.disableTipsForCurrentSession = false;
  await setDefaultEngine("Google");
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    await withDNSRedirect("www.google.com", "/", async url => {
      BrowserTestUtils.startLoadingURIString(gBrowser.selectedBrowser, url);
      await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
      await checkTip(window, UrlbarProviderSearchTips.TIP_TYPE.REDIRECT, false);

      // Click in the input.
      await UrlbarTestUtils.promisePopupClose(window, () => {
        EventUtils.synthesizeMouseAtCenter(gURLBar.textbox.parentNode, {});
      });
      gURLBar.blur();
    });
  });

  // Check telemetry.
  const scalars = TelemetryTestUtils.getProcessScalars("parent", true, true);
  TelemetryTestUtils.assertKeyedScalar(
    scalars,
    "urlbar.tips",
    `${UrlbarProviderSearchTips.TIP_TYPE.REDIRECT}-picked`,
    1
  );

  Assert.equal(
    UrlbarPrefs.get(
      `tipShownCount.${UrlbarProviderSearchTips.TIP_TYPE.REDIRECT}`
    ),
    MAX_SHOWN_COUNT,
    "Redirect tips are disabled after tip button is picked."
  );
  Assert.equal(gURLBar.value, "", "The Urlbar should be empty.");
  resetSearchTipsProvider();
});

// Clicking in the input while the persist tip is showing should have the same
// effect as picking the tip.
add_task(async function clickInInput_persist() {
  UrlbarProviderSearchTips.disableTipsForCurrentSession = false;
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.showSearchTerms.featureGate", true]],
  });

  await setDefaultEngine("Example");
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    let browserLoadedPromise = BrowserTestUtils.browserLoaded(
      gBrowser.selectedBrowser,
      false,
      SEARCH_SERP_URL
    );
    BrowserTestUtils.startLoadingURIString(
      gBrowser.selectedBrowser,
      SEARCH_SERP_URL
    );
    await browserLoadedPromise;
    await checkTip(window, UrlbarProviderSearchTips.TIP_TYPE.PERSIST, false);

    // Click in the input.
    await UrlbarTestUtils.promisePopupClose(window, () => {
      EventUtils.synthesizeMouseAtCenter(gURLBar.textbox.parentNode, {});
    });
    gURLBar.blur();
    Assert.equal(
      gURLBar.value,
      SEARCH_TERM,
      "The Urlbar should keep its existing value."
    );
  });

  // Check telemetry.
  const scalars = TelemetryTestUtils.getProcessScalars("parent", true, true);
  TelemetryTestUtils.assertKeyedScalar(
    scalars,
    "urlbar.tips",
    `${UrlbarProviderSearchTips.TIP_TYPE.PERSIST}-picked`,
    1
  );

  Assert.equal(
    UrlbarPrefs.get(
      `tipShownCount.${UrlbarProviderSearchTips.TIP_TYPE.PERSIST}`
    ),
    MAX_SHOWN_COUNT,
    "Persist tips are disabled after tip button is picked."
  );
  Assert.equal(gURLBar.value, "", "The Urlbar should be empty.");
  resetSearchTipsProvider();
});

// Pressing Ctrl+L (the open location command) while the redirect tip is showing
// should have the same effect as picking the tip.
add_task(async function openLocation_redirect() {
  UrlbarProviderSearchTips.disableTipsForCurrentSession = false;
  await setDefaultEngine("Google");
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    await withDNSRedirect("www.google.com", "/", async url => {
      BrowserTestUtils.startLoadingURIString(gBrowser.selectedBrowser, url);
      await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
      await checkTip(window, UrlbarProviderSearchTips.TIP_TYPE.REDIRECT, false);

      // Trigger the open location command.
      await UrlbarTestUtils.promisePopupClose(window, () => {
        document.getElementById("Browser:OpenLocation").doCommand();
      });
      gURLBar.blur();
    });
  });

  // Check telemetry.
  const scalars = TelemetryTestUtils.getProcessScalars("parent", true, true);
  TelemetryTestUtils.assertKeyedScalar(
    scalars,
    "urlbar.tips",
    `${UrlbarProviderSearchTips.TIP_TYPE.REDIRECT}-picked`,
    1
  );

  Assert.equal(
    UrlbarPrefs.get(
      `tipShownCount.${UrlbarProviderSearchTips.TIP_TYPE.REDIRECT}`
    ),
    MAX_SHOWN_COUNT,
    "Redirect tips are disabled after tip button is picked."
  );
  Assert.equal(gURLBar.value, "", "The Urlbar should be empty.");
  resetSearchTipsProvider();
});

// Pressing Ctrl+L (the open location command) while the persist tip is showing
// should have the same effect as picking the tip.
add_task(async function openLocation_persist() {
  UrlbarProviderSearchTips.disableTipsForCurrentSession = false;
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.showSearchTerms.featureGate", true]],
  });
  await setDefaultEngine("Example");
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    let browserLoadedPromise = BrowserTestUtils.browserLoaded(
      gBrowser.selectedBrowser,
      false,
      SEARCH_SERP_URL
    );
    BrowserTestUtils.startLoadingURIString(
      gBrowser.selectedBrowser,
      SEARCH_SERP_URL
    );
    await browserLoadedPromise;
    await checkTip(window, UrlbarProviderSearchTips.TIP_TYPE.PERSIST, false);

    // Trigger the open location command.
    await UrlbarTestUtils.promisePopupClose(window, () => {
      document.getElementById("Browser:OpenLocation").doCommand();
    });
    gURLBar.blur();
    Assert.equal(
      gURLBar.value,
      SEARCH_TERM,
      "The Urlbar should keep its existing value."
    );
  });

  // Check telemetry.
  const scalars = TelemetryTestUtils.getProcessScalars("parent", true, true);
  TelemetryTestUtils.assertKeyedScalar(
    scalars,
    "urlbar.tips",
    `${UrlbarProviderSearchTips.TIP_TYPE.PERSIST}-picked`,
    1
  );

  Assert.equal(
    UrlbarPrefs.get(
      `tipShownCount.${UrlbarProviderSearchTips.TIP_TYPE.PERSIST}`
    ),
    MAX_SHOWN_COUNT,
    "Persist tips are disabled after tip button is picked."
  );
  Assert.equal(gURLBar.value, "", "The Urlbar should be empty.");
  resetSearchTipsProvider();
  await SpecialPowers.popPrefEnv();
});

add_task(async function pickingTipDoesNotDisableOtherKinds() {
  UrlbarProviderSearchTips.disableTipsForCurrentSession = false;
  await setDefaultEngine("Google");
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:newtab",
    waitForLoad: false,
  });
  await checkTip(window, UrlbarProviderSearchTips.TIP_TYPE.ONBOARD, false);

  // Click the tip button.
  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  let button = result.element.row._buttons.get("0");
  await UrlbarTestUtils.promisePopupClose(window, () => {
    EventUtils.synthesizeMouseAtCenter(button, {});
  });

  gURLBar.blur();
  Assert.equal(
    UrlbarPrefs.get(
      `tipShownCount.${UrlbarProviderSearchTips.TIP_TYPE.ONBOARD}`
    ),
    MAX_SHOWN_COUNT,
    "Onboarding tips are disabled after tip button is picked."
  );

  BrowserTestUtils.removeTab(tab);

  // Simulate a new session.
  UrlbarProviderSearchTips.disableTipsForCurrentSession = false;

  // Onboarding tips should no longer be shown.
  let tab2 = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:newtab",
    waitForLoad: false,
  });
  await checkTip(window, UrlbarProviderSearchTips.TIP_TYPE.NONE);

  // We should still show redirect tips.
  await withDNSRedirect("www.google.com", "/", async url => {
    await checkTab(window, url, UrlbarProviderSearchTips.TIP_TYPE.REDIRECT);
  });

  BrowserTestUtils.removeTab(tab2);
  resetSearchTipsProvider();
});

// The tip shouldn't be shown when there's another notification present.
add_task(async function notification() {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    let box = gBrowser.getNotificationBox();
    let note = box.appendNotification("urlbar-test", {
      label: "Test",
      priority: box.PRIORITY_INFO_HIGH,
    });
    // Give it a big persistence so it doesn't go away on page load.
    note.persistence = 100;
    await withDNSRedirect("www.google.com", "/", async url => {
      BrowserTestUtils.startLoadingURIString(gBrowser.selectedBrowser, url);
      await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
      await checkTip(window, UrlbarProviderSearchTips.TIP_TYPE.NONE);
      box.removeNotification(note, true);
    });
  });
  resetSearchTipsProvider();
});

// The tip should be shown when switching to a tab where it should be shown.
add_task(async function tabSwitch() {
  let tab = BrowserTestUtils.addTab(gBrowser, "about:newtab");
  UrlbarProviderSearchTips.disableTipsForCurrentSession = false;
  Services.telemetry.clearScalars();
  await BrowserTestUtils.switchTab(gBrowser, tab);
  await checkTip(window, UrlbarProviderSearchTips.TIP_TYPE.ONBOARD);
  BrowserTestUtils.removeTab(tab);
  resetSearchTipsProvider();
});

// The engagement event should be ended if the user ignores a tip.
// See bug 1610024.
add_task(async function ignoreEndsEngagement() {
  await setDefaultEngine("Google");
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    await withDNSRedirect("www.google.com", "/", async url => {
      BrowserTestUtils.startLoadingURIString(gBrowser.selectedBrowser, url);
      await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
      await checkTip(window, UrlbarProviderSearchTips.TIP_TYPE.REDIRECT, false);
      // We're just looking for any target outside the Urlbar.
      let spring = gURLBar.inputField
        .closest("#nav-bar")
        .querySelector("toolbarspring");
      await UrlbarTestUtils.promisePopupClose(window, async () => {
        await EventUtils.synthesizeMouseAtCenter(spring, {});
      });
      Assert.ok(
        UrlbarProviderSearchTips.showedTipTypeInCurrentEngagement ==
          UrlbarProviderSearchTips.TIP_TYPE.NONE,
        "The engagement should have ended after the tip was ignored."
      );
    });
  });
  resetSearchTipsProvider();
});

add_task(async function pasteAndGo_url() {
  await doPasteAndGoTest("http://example.com/", "http://example.com/");
});

add_task(async function pasteAndGo_nonURL() {
  await setDefaultEngine("Example");
  await doPasteAndGoTest(
    "pasteAndGo_nonURL",
    "https://example.com/?q=pasteAndGo_nonURL"
  );
  await setDefaultEngine("Google");
});

async function doPasteAndGoTest(searchString, expectedURL) {
  UrlbarProviderSearchTips.disableTipsForCurrentSession = false;
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:newtab",
    waitForLoad: false,
  });
  await checkTip(window, UrlbarProviderSearchTips.TIP_TYPE.ONBOARD, false);

  await SimpleTest.promiseClipboardChange(searchString, () => {
    clipboardHelper.copyString(searchString);
  });

  let textBox = gURLBar.querySelector("moz-input-box");
  let cxmenu = textBox.menupopup;
  let cxmenuPromise = BrowserTestUtils.waitForEvent(cxmenu, "popupshown");
  EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, {
    type: "contextmenu",
    button: 2,
  });
  await cxmenuPromise;
  let menuitem = textBox.getMenuItem("paste-and-go");

  let browserLoadedPromise = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    expectedURL
  );
  cxmenu.activateItem(menuitem);
  await browserLoadedPromise;
  BrowserTestUtils.removeTab(tab);
  resetSearchTipsProvider();
}

// Since we coupled the logic that decides whether to show the tip with our
// gURLBar.search call, we should make sure search isn't called when
// the conditions for a tip are met but the provider is disabled.
add_task(async function noActionWhenDisabled() {
  await setDefaultEngine("Bing");
  await withDNSRedirect("www.bing.com", "/", async url => {
    await checkTab(window, url, UrlbarProviderSearchTips.TIP_TYPE.REDIRECT);
  });

  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "browser.newtabpage.activity-stream.asrouter.userprefs.cfr.features",
        false,
      ],
    ],
  });

  await withDNSRedirect("www.bing.com", "/", async url => {
    Assert.ok(
      !UrlbarTestUtils.isPopupOpen(window),
      "The UrlbarView should not be open."
    );
  });

  await SpecialPowers.popPrefEnv();
});
