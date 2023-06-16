/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/* import-globals-from helper-addons.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "helper-addons.js", this);

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.eventPages.enabled", true]],
  });
});

// Test that the terminate button is shutting down the background script as expected
// and the background script status is updated occordingly.
add_task(async function test_eventpage_terminate_and_status_updates() {
  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  const EXTENSION_ID = "test-devtools-eventpage@mozilla.org";
  const EXTENSION_NAME = "Temporary EventPage-based web extension";

  const EXTENSION2_ID = "test-devtools-persistentbg@mozilla.org";
  const EXTENSION2_NAME =
    "Temporary PersistentBackgroundPage-based web extension";

  const promiseBackgroundLoaded = promiseBackgroundContextLoaded(EXTENSION_ID);
  const promiseBackgroundUnloaded =
    promiseBackgroundContextUnloaded(EXTENSION_ID);

  let waitForBGStatusUpdate = promiseBackgroundStatusUpdate(window);

  // Install the extension using an event page (non persistent background page).
  await installTemporaryExtensionFromXPI(
    {
      id: EXTENSION_ID,
      name: EXTENSION_NAME,
      // The extension is expected to have a non persistent background script.
      extraProperties: {
        background: {
          scripts: ["bgpage.js"],
          persistent: false,
        },
      },
      files: {
        "bgpage.js": function () {
          // Emit a dump when the script is loaded to make it easier
          // to investigate intermittents.
          dump(`Background script loaded: ${window.location}\n`);
        },
      },
    },
    document
  );

  // Install the extension using a persistent background page.
  await installTemporaryExtensionFromXPI(
    {
      id: EXTENSION2_ID,
      name: EXTENSION2_NAME,
      // The extension is expected to have a persistent background script.
      extraProperties: {
        background: {
          page: "bppage.html",
          persistent: true,
        },
      },
      files: { "bgpage.html": "" },
    },
    document
  );

  const target = findDebugTargetByText(EXTENSION_NAME, document);
  ok(
    !!target,
    "The EventPage-based extension is installed with the expected name"
  );

  const target2 = findDebugTargetByText(EXTENSION2_NAME, document);
  ok(
    !!target2,
    "The PersistentBackgroundScript-based extension is installed with the expected name"
  );

  const terminateButton = target.querySelector(
    ".qa-temporary-extension-terminate-bgscript-button"
  );
  ok(
    !!terminateButton,
    `${EXTENSION_NAME} is expected to have a terminate button`
  );

  const terminateButton2 = target2.querySelector(
    ".qa-temporary-extension-terminate-bgscript-button"
  );
  ok(
    !terminateButton2,
    `${EXTENSION2_NAME} is expected to not have a terminate button`
  );

  info("Wait for the test extension background script to be loaded");
  await promiseBackgroundLoaded;

  info("Wait for the test extension background script status update");
  await waitForBGStatusUpdate;

  await assertBackgroundStatus(EXTENSION_NAME, {
    document,
    expectedStatus: "running",
  });

  // Verify in the card related to extensions with a persistent background page
  // the background script status is not being rendered at all.
  const backgroundStatus2 = target2.querySelector(
    ".extension-backgroundscript__status"
  );
  ok(
    !backgroundStatus2,
    `${EXTENSION2_NAME} should not be showing background script status`
  );

  info(`Click on the terminate button for ${EXTENSION_NAME}`);
  const waitForTerminateSuccess = waitForDispatch(
    window.AboutDebugging.store,
    "TERMINATE_EXTENSION_BGSCRIPT_SUCCESS"
  );
  waitForBGStatusUpdate = promiseBackgroundStatusUpdate(window);
  terminateButton.click();
  await waitForTerminateSuccess;

  info("Wait for the extension background script to be unloaded");
  await promiseBackgroundUnloaded;
  await waitForBGStatusUpdate;
  await assertBackgroundStatus(EXTENSION_NAME, {
    document,
    expectedStatus: "stopped",
  });

  // Uninstall the test extensions.
  await Promise.all([
    AddonManager.getAddonByID(EXTENSION_ID).then(addon => addon.uninstall()),
    AddonManager.getAddonByID(EXTENSION2_ID).then(addon => addon.uninstall()),
  ]);

  info("Wait until the debug targets with test extensions disappears");
  await waitUntil(
    () =>
      !findDebugTargetByText(EXTENSION_NAME, document) &&
      !findDebugTargetByText(EXTENSION2_NAME, document)
  );

  await removeTab(tab);
});
