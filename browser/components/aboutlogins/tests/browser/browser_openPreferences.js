/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

let { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);

add_setup(async function () {
  await TestUtils.waitForCondition(() => {
    Services.telemetry.clearEvents();
    let events = Services.telemetry.snapshotEvents(
      Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
      true
    ).content;
    return !events || !events.length;
  }, "Waiting for content telemetry events to get cleared");

  await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:logins",
  });
  registerCleanupFunction(() => {
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  });
});

add_task(async function test_open_preferences() {
  // We want to make sure we visit about:preferences#privacy-logins , as that is
  // what causes us to scroll to and highlight the "logins" section. However,
  // about:preferences will redirect the URL, so the eventual load event will happen
  // on about:preferences#privacy . The `wantLoad` parameter we pass to
  // `waitForNewTab` needs to take this into account:
  let seenFirstURL = false;
  let promiseNewTab = BrowserTestUtils.waitForNewTab(
    gBrowser,
    url => {
      if (url == "about:preferences#privacy-logins") {
        seenFirstURL = true;
        return true;
      } else if (url == "about:preferences#privacy") {
        Assert.ok(
          seenFirstURL,
          "Must have seen an onLocationChange notification for the privacy-logins hash"
        );
        return true;
      }
      return false;
    },
    true
  );

  let browser = gBrowser.selectedBrowser;
  await BrowserTestUtils.synthesizeMouseAtCenter("menu-button", {}, browser);
  await SpecialPowers.spawn(browser, [], async () => {
    return ContentTaskUtils.waitForCondition(() => {
      let menuButton = Cu.waiveXrays(
        content.document.querySelector("menu-button")
      );
      return !menuButton.shadowRoot.querySelector(".menu").hidden;
    }, "waiting for menu to open");
  });

  function getPrefsItem() {
    let menuButton = window.document.querySelector("menu-button");
    return menuButton.shadowRoot.querySelector(".menuitem-preferences");
  }
  await BrowserTestUtils.synthesizeMouseAtCenter(getPrefsItem, {}, browser);

  info("waiting for new tab to get opened");
  let newTab = await promiseNewTab;
  Assert.ok(true, "New tab opened to about:preferences");

  BrowserTestUtils.removeTab(newTab);

  // First event is for opening about:logins
  await LoginTestUtils.telemetry.waitForEventCount(2);
  TelemetryTestUtils.assertEvents(
    [["pwmgr", "mgmt_menu_item_used", "preferences"]],
    { category: "pwmgr", method: "mgmt_menu_item_used" },
    { process: "content" }
  );
});
