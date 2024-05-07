/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Test the export logins file picker appears.
 */

let { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);

let { MockFilePicker } = SpecialPowers;

add_setup(async function () {
  await TestUtils.waitForCondition(() => {
    Services.telemetry.clearEvents();
    let events = Services.telemetry.snapshotEvents(
      Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
      true
    ).content;
    return !events || !events.length;
  }, "Waiting for content telemetry events to get cleared");

  MockFilePicker.init(window.browsingContext);
  MockFilePicker.useAnyFile();
  MockFilePicker.returnValue = MockFilePicker.returnOK;

  registerCleanupFunction(() => {
    MockFilePicker.cleanup();
  });
});

function waitForFilePicker() {
  return new Promise(resolve => {
    MockFilePicker.showCallback = () => {
      MockFilePicker.showCallback = null;
      Assert.ok(true, "Saw the file picker");
      resolve();
    };
  });
}

add_task(async function test_open_export() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:logins" },
    async function (browser) {
      await BrowserTestUtils.synthesizeMouseAtCenter(
        "menu-button",
        {},
        browser
      );

      await SpecialPowers.spawn(browser, [], async () => {
        let menuButton = content.document.querySelector("menu-button");
        return ContentTaskUtils.waitForCondition(function waitForMenu() {
          return !menuButton.shadowRoot.querySelector(".menu").hidden;
        }, "waiting for menu to open");
      });

      function getExportMenuItem() {
        let menuButton = window.document.querySelector("menu-button");
        let exportButton =
          menuButton.shadowRoot.querySelector(".menuitem-export");
        return exportButton;
      }

      await BrowserTestUtils.synthesizeMouseAtCenter(
        getExportMenuItem,
        {},
        browser
      );

      // First event is for opening about:logins
      await LoginTestUtils.telemetry.waitForEventCount(2);
      TelemetryTestUtils.assertEvents(
        [["pwmgr", "mgmt_menu_item_used", "export"]],
        { category: "pwmgr", method: "mgmt_menu_item_used" },
        { process: "content" }
      );

      info("Clicking confirm button");
      let osReAuthPromise = null;

      if (
        OSKeyStore.canReauth() &&
        !OSKeyStoreTestUtils.canTestOSKeyStoreLogin()
      ) {
        todo(
          OSKeyStoreTestUtils.canTestOSKeyStoreLogin(),
          "Cannot test OS key store login in this build."
        );
        return;
      }

      if (OSKeyStore.canReauth()) {
        osReAuthPromise = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(true);
      }
      let filePicker = waitForFilePicker();
      await BrowserTestUtils.synthesizeMouseAtCenter(
        () => {
          let confirmExportDialog = window.document.querySelector(
            "confirmation-dialog"
          );
          return confirmExportDialog.shadowRoot.querySelector(
            ".confirm-button"
          );
        },
        {},
        browser
      );

      if (osReAuthPromise) {
        Assert.ok(osReAuthPromise, "Waiting for OS re-auth promise");
        await osReAuthPromise;
      }

      info("waiting for Export file picker to get opened");
      await filePicker;
      Assert.ok(true, "Export file picker opened");

      info("Waiting for the export to complete");
      let expectedEvents = [
        [
          "pwmgr",
          "reauthenticate",
          "os_auth",
          osReAuthPromise ? "success" : "success_unsupported_platform",
        ],
        ["pwmgr", "mgmt_menu_item_used", "export_complete"],
      ];
      await LoginTestUtils.telemetry.waitForEventCount(
        expectedEvents.length,
        "parent"
      );

      TelemetryTestUtils.assertEvents(
        expectedEvents,
        { category: "pwmgr", method: /(reauthenticate|mgmt_menu_item_used)/ },
        { process: "parent" }
      );
    }
  );
});
