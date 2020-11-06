/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

let { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

let { MockFilePicker } = SpecialPowers;

function waitForFilePicker() {
  return new Promise(resolve => {
    MockFilePicker.showCallback = () => {
      MockFilePicker.showCallback = null;
      ok(true, "Saw the file picker");
      resolve();
    };
  });
}

add_task(async function setup() {
  MockFilePicker.init(window);
  MockFilePicker.useAnyFile();
  MockFilePicker.returnValue = MockFilePicker.returnOK;
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
    MockFilePicker.cleanup();
  });
});

add_task(async function test_open_import() {
  let promiseImportWindow = BrowserTestUtils.domWindowOpenedAndLoaded();

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

  info("waiting for Import to get opened");
  let importWindow = await promiseImportWindow;
  ok(true, "Import opened");

  let filePicker = waitForFilePicker();
  info("waiting for Import file picker to get opened");
  await filePicker;
  ok(true, "Import file picker opened");

  await BrowserTestUtils.synthesizeMouseAtCenter(
    () => {
      let confirmExportDialog = window.document.querySelector(
        "import-summary-dialog"
      );
      return confirmExportDialog.shadowRoot.querySelector(
        ".import-done-button"
      );
    },
    {},
    browser
  );

  // First event is for opening about:logins
  await LoginTestUtils.telemetry.waitForEventCount(2);
  TelemetryTestUtils.assertEvents(
    [["pwmgr", "mgmt_menu_item_used", "import_from_browser"]],
    { category: "pwmgr", method: "mgmt_menu_item_used" },
    { process: "content" }
  );

  importWindow.close();
});
