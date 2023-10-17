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

  let aboutLoginsTab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:logins",
  });
  registerCleanupFunction(() => {
    BrowserTestUtils.removeTab(aboutLoginsTab);
  });
});

add_task(async function test_open_import() {
  let promiseWizardTab = BrowserTestUtils.waitForMigrationWizard(window);

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

  function getImportItem() {
    let menuButton = window.document.querySelector("menu-button");
    return menuButton.shadowRoot.querySelector(".menuitem-import-browser");
  }
  await BrowserTestUtils.synthesizeMouseAtCenter(getImportItem, {}, browser);

  info("waiting for migration wizard to open");
  let wizardTab = await promiseWizardTab;
  Assert.ok(wizardTab, "Migration wizard opened");

  // First event is for opening about:logins
  await LoginTestUtils.telemetry.waitForEventCount(2);
  TelemetryTestUtils.assertEvents(
    [["pwmgr", "mgmt_menu_item_used", "import_from_browser"]],
    { category: "pwmgr", method: "mgmt_menu_item_used" },
    { process: "content" }
  );

  await BrowserTestUtils.removeTab(wizardTab);
});
