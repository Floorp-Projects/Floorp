/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_setup(async function() {
  if (!Services.prefs.getBoolPref("browser.tabs.firefox-view")) {
    await SpecialPowers.pushPrefEnv({
      set: [["browser.tabs.firefox-view", true]],
    });
    CustomizableUI.addWidgetToArea(
      "firefox-view-button",
      CustomizableUI.AREA_TABSTRIP,
      0
    );
    registerCleanupFunction(() => {
      CustomizableUI.removeWidgetFromArea("firefox-view-button");
    });
  }
});

function assertFirefoxViewTab(w = window) {
  ok(w.gFirefoxViewTab, "Firefox View tab exists");
  ok(w.gFirefoxViewTab.hidden, "Firefox View tab is hidden");
  is(
    w.gFirefoxViewTab,
    w.gBrowser.tabs[0],
    "Firefox View tab is the first tab"
  );
}

async function openFirefoxViewTab(w = window) {
  ok(
    !w.gFirefoxViewTab,
    "Firefox View tab doesn't exist prior to clicking the button"
  );
  info("Clicking the Firefox View button");
  await EventUtils.synthesizeMouseAtCenter(
    w.document.getElementById("firefox-view-button"),
    {}
  );
  assertFirefoxViewTab(w);
  is(w.gFirefoxViewTab, w.gBrowser.selectedTab, "Firefox View tab is selected");
  await BrowserTestUtils.browserLoaded(gFirefoxViewTab.linkedBrowser);
}

function closeFirefoxViewTab(w = window) {
  w.gBrowser.removeTab(w.gFirefoxViewTab);
  ok(
    !w.gFirefoxViewTab,
    "Reference to Firefox View tab got removed when closing the tab"
  );
}

add_task(async function openLinkIn() {
  await openFirefoxViewTab();
  gURLBar.focus();
  gURLBar.value = "https://example.com";
  let newTabOpened = BrowserTestUtils.waitForEvent(
    gBrowser.tabContainer,
    "TabOpen"
  );
  await EventUtils.synthesizeKey("KEY_Enter");
  info(
    "Waiting for new tab to open from the address bar in the Firefox View tab"
  );
  await newTabOpened;
  assertFirefoxViewTab();
  isnot(
    gFirefoxViewTab,
    gBrowser.selectedTab,
    "Firefox View tab is not selected anymore (new tab opened in the foreground)"
  );
  gBrowser.removeCurrentTab();
  closeFirefoxViewTab();
});
