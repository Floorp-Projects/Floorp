"use strict";

const TELEMETRY_EVENT = "security#javascriptLoad#parentProcess";

add_task(async function test_contentscript_telemetry() {
  // Turn on telemetry and reset it to the previous state once the test is completed.
  // const telemetryCanRecordBase = Services.telemetry.canRecordBase;
  // Services.telemetry.canRecordBase = true;
  // SimpleTest.registerCleanupFunction(() => {
  //   Services.telemetry.canRecordBase = telemetryCanRecordBase;
  // });

  function background() {
    browser.test.onMessage.addListener(async msg => {
      if (msg !== "execute") {
        return;
      }
      browser.tabs.executeScript({
        file: "execute_script.js",
        allFrames: true,
        matchAboutBlank: true,
        runAt: "document_start",
      });

      await browser.userScripts.register({
        js: [{ file: "user_script.js" }],
        matches: ["<all_urls>"],
        matchAboutBlank: true,
        allFrames: true,
        runAt: "document_start",
      });

      await browser.contentScripts.register({
        js: [{ file: "content_script.js" }],
        matches: ["<all_urls>"],
        matchAboutBlank: true,
        allFrames: true,
        runAt: "document_start",
      });

      browser.test.sendMessage("executed");
    });
  }

  let extensionData = {
    manifest: {
      permissions: ["tabs", "<all_urls>"],
      user_scripts: {},
    },
    background,
    files: {
      // Fail if this ever executes.
      "execute_script.js": 'browser.test.fail("content-script-run");',
      "user_script.js": 'browser.test.fail("content-script-run");',
      "content_script.js": 'browser.test.fail("content-script-run");',
    },
  };

  function getSecurityEventCount() {
    let snap = Services.telemetry.getSnapshotForKeyedScalars();
    return snap.parent["telemetry.event_counts"][TELEMETRY_EVENT] || 0;
  }

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:preferences"
  );
  is(
    getSecurityEventCount(),
    0,
    `No events recorded before startup: ${TELEMETRY_EVENT}.`
  );

  let extension = ExtensionTestUtils.loadExtension(extensionData);

  await extension.startup();
  is(
    getSecurityEventCount(),
    0,
    `No events recorded after startup: ${TELEMETRY_EVENT}.`
  );

  extension.sendMessage("execute");
  await extension.awaitMessage("executed");

  // Do another load.
  BrowserTestUtils.removeTab(tab);
  tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:preferences"
  );

  BrowserTestUtils.removeTab(tab);
  await extension.unload();

  is(
    getSecurityEventCount(),
    0,
    `No events recorded after executeScript: ${TELEMETRY_EVENT}.`
  );
});
