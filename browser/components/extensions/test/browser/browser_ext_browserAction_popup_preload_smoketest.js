/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* eslint-disable mozilla/no-arbitrary-setTimeout */
"use strict";

// This test does create and cancel the preloaded popup
// multiple times and in some cases it takes longer than
// the default timeouts allows.
requestLongerTimeout(4);

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);

async function installTestAddon(addonId, unpacked = false) {
  let xpi = AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      browser_specific_settings: { gecko: { id: addonId } },
      browser_action: {
        default_popup: "popup.html",
        default_area: "navbar",
      },
    },
    files: {
      "popup.html": `<!DOCTYPE html>
      <html>
        <head>
          <meta charset="utf-8">
            <link rel="stylesheet" href="popup.css">
            </head>
            <body>
            </body>
          </html>
      `,
      "popup.css": `@import "imported.css";`,
      "imported.css": `
        /* Increasing the imported.css file size to increase the
         * chances to trigger the stylesheet preload issue that
         * has been fixed by Bug 1735899 consistently and with
         * a smaller number of preloaded popup cancelled.
         *
         * ${new Array(600000).fill("x").join("\n")} 
         */
        body { width: 300px; height: 300px; background: red; }
      `,
    },
  });

  if (unpacked) {
    // This temporary directory is going to be removed from the
    // cleanup function, but also make it unique as we do for the
    // other temporary files (e.g. like getTemporaryFile as defined
    // in XPIInstall.sys.mjs).
    const random = Math.round(Math.random() * 36 ** 3).toString(36);
    const tmpDirName = `mochitest_unpacked_addons_${random}`;
    let tmpExtPath = FileUtils.getDir("TmpD", [tmpDirName]);
    tmpExtPath.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
    registerCleanupFunction(() => {
      tmpExtPath.remove(true);
    });

    // Unpacking the xpi file into the tempoary directory.
    const extDir = await AddonTestUtils.manuallyInstall(
      xpi,
      tmpExtPath,
      null,
      /* unpacked */ true
    );

    // Install temporarily as unpacked.
    return AddonManager.installTemporaryAddon(extDir);
  }

  // Install temporarily as packed.
  return AddonManager.installTemporaryAddon(xpi);
}

async function waitForExtensionAndBrowserAction(addonId) {
  const {
    Management: {
      global: { browserActionFor },
    },
  } = ChromeUtils.importESModule("resource://gre/modules/Extension.sys.mjs");

  // trigger a number of preloads
  let extension;
  let browserAction;
  await TestUtils.waitForCondition(() => {
    extension = WebExtensionPolicy.getByID(addonId)?.extension;
    browserAction = extension && browserActionFor(extension);
    return browserAction;
  }, "got the extension and browserAction");

  let widget = getBrowserActionWidget(extension).forWindow(window);

  return { extension, browserAction, widget };
}

async function testCancelPreloadedPopup({ browserAction, widget }) {
  // Trigger the preloaded popup.
  EventUtils.synthesizeMouseAtCenter(
    widget.node,
    { type: "mouseover", button: 0 },
    window
  );
  await TestUtils.waitForCondition(
    () => browserAction.pendingPopup,
    "Wait for the preloaded popup"
  );
  // Cancel the preloaded popup.
  EventUtils.synthesizeMouseAtCenter(
    widget.node,
    { type: "mouseout", button: 0 },
    window
  );
  EventUtils.synthesizeMouseAtCenter(
    window.gURLBar.textbox,
    { type: "mouseover" },
    window
  );
  await TestUtils.waitForCondition(
    () => !browserAction.pendingPopup,
    "Wait for the preloaded popup to be cancelled"
  );
}

async function testPopupLoadCompleted({ extension, browserAction, widget }) {
  const promiseViewShowing = BrowserTestUtils.waitForEvent(
    document,
    "ViewShowing",
    false,
    ev => ev.target.id === browserAction.viewId
  );
  EventUtils.synthesizeMouseAtCenter(
    widget.node,
    { type: "mousedown", button: 0 },
    window
  );
  EventUtils.synthesizeMouseAtCenter(
    widget.node,
    { type: "mouseup", button: 0 },
    window
  );

  info("Await the browserAction popup to be shown");
  await promiseViewShowing;

  info("Await the browserAction popup to be fully loaded");
  const browser = await awaitExtensionPanel(
    extension,
    window,
    /* awaitLoad */ true
  );

  await TestUtils.waitForCondition(async () => {
    const docReadyState = await SpecialPowers.spawn(browser, [], () => {
      return this.content.document.readyState;
    });

    return docReadyState === "complete";
  }, "Wait the popup document to get to readyState complete");

  ok(true, "Popup document was fully loaded");
}

// This test is covering a scenario similar to the one fixed in Bug 1735899,
// and possibly some other similar ones that may slip through unnoticed.
add_task(async function testCancelPopupPreloadRaceOnUnpackedAddon() {
  const ID = "preloaded-popup@test";
  const addon = await installTestAddon(ID, /* unpacked */ true);
  const { extension, browserAction, widget } =
    await waitForExtensionAndBrowserAction(ID);
  info("Preload popup and cancel it multiple times");
  for (let i = 0; i < 200; i++) {
    await testCancelPreloadedPopup({ browserAction, widget });
  }
  await testPopupLoadCompleted({ extension, browserAction, widget });
  await addon.uninstall();
});

// This test is covering a scenario similar to the one fixed in Bug 1735899,
// and possibly some other similar ones that may slip through unnoticed.
add_task(async function testCancelPopupPreloadRaceOnPackedAddon() {
  const ID = "preloaded-popup@test";
  const addon = await installTestAddon(ID, /* unpacked */ false);
  const { extension, browserAction, widget } =
    await waitForExtensionAndBrowserAction(ID);
  info("Preload popup and cancel it multiple times");
  for (let i = 0; i < 200; i++) {
    await testCancelPreloadedPopup({ browserAction, widget });
  }
  await testPopupLoadCompleted({ extension, browserAction, widget });
  await addon.uninstall();
});
