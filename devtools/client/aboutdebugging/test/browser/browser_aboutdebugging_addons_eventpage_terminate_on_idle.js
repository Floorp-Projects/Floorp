/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/* import-globals-from helper-addons.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "helper-addons.js", this);

add_setup(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.eventPages.enabled", true]],
  });
});

// Test that an extension event page isn't terminated on idle when a DevTools
// Toolbox is attached to the extension.
add_task(
  async function test_eventpage_no_idle_shutdown_with_toolbox_attached() {
    const { document, tab, window } = await openAboutDebugging();
    await selectThisFirefoxPage(document, window.AboutDebugging.store);

    const EXTENSION_ID = "test-devtools-eventpage@mozilla.org";
    const EXTENSION_NAME = "Temporary EventPage-based web extension";

    const promiseBackgroundLoaded = promiseBackgroundContextLoaded(
      EXTENSION_ID
    );

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
          "bgpage.js": function() {
            // Emit a dump when the script is loaded to make it easier
            // to investigate intermittents.
            dump(`Background script loaded: ${window.location}\n`);
          },
        },
      },
      document
    );

    const target = findDebugTargetByText(EXTENSION_NAME, document);
    ok(
      !!target,
      "The EventPage-based extension is installed with the expected name"
    );

    info("Wait for the test extension background script to be loaded");
    await promiseBackgroundLoaded;

    info("Wait for the test extension background script status update");
    await waitForBGStatusUpdate;
    await assertBackgroundStatus(EXTENSION_NAME, {
      document,
      expectedStatus: "running",
    });

    waitForBGStatusUpdate = promiseBackgroundStatusUpdate(window);
    await triggerExtensionEventPageIdleTimeout(EXTENSION_ID);
    await waitForBGStatusUpdate;
    await assertBackgroundStatus(EXTENSION_NAME, {
      document,
      expectedStatus: "stopped",
    });

    info(
      "Respawn the extension background script on new WebExtension API event"
    );
    waitForBGStatusUpdate = promiseBackgroundStatusUpdate(window);
    await wakeupExtensionEventPage(EXTENSION_ID);
    await waitForBGStatusUpdate;
    await assertBackgroundStatus(EXTENSION_NAME, {
      document,
      expectedStatus: "running",
    });

    info("Open a DevTools toolbox on the target extension");
    const { devtoolsTab, devtoolsWindow } = await openAboutDevtoolsToolbox(
      document,
      tab,
      window,
      EXTENSION_NAME
    );

    info(
      "Verify event page terminated on terminate button clicked while the DevTools toolbox is open"
    );
    const terminateButton = target.querySelector(
      ".qa-temporary-extension-terminate-bgscript-button"
    );
    ok(
      !!terminateButton,
      `${EXTENSION_NAME} is expected to have a terminate button`
    );

    info(`Click on the terminate button for ${EXTENSION_NAME}`);
    const promiseBackgroundUnloaded = promiseBackgroundContextUnloaded(
      EXTENSION_ID
    );
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
      targetElement: target,
    });

    info(
      "Verify event page isn't terminated on idle while the DevTools toolbox is open"
    );

    // Make sure the event page is running again.
    waitForBGStatusUpdate = promiseBackgroundStatusUpdate(window);
    await wakeupExtensionEventPage(EXTENSION_ID);
    await waitForBGStatusUpdate;
    await assertBackgroundStatus(EXTENSION_NAME, {
      document,
      expectedStatus: "running",
      targetElement: target,
    });

    const waitForBGSuspendIgnored = promiseTerminateBackgroundScriptIgnored(
      EXTENSION_ID
    );
    waitForBGStatusUpdate = promiseBackgroundStatusUpdate(window);
    await triggerExtensionEventPageIdleTimeout(EXTENSION_ID);
    await Promise.race([waitForBGStatusUpdate, waitForBGSuspendIgnored]);

    await assertBackgroundStatus(EXTENSION_NAME, {
      document,
      expectedStatus: "running",
      // After opening the toolbox there will be an additional target listed
      // for the devtools toolbox tab, its card includes the extension name
      // and so while the toolbox is open we should make sure to look for
      // the background status inside the extension target card instead of
      // the one associated to the devtools toolbox tab.
      targetElement: target,
    });

    info(
      "Wait for warning message expected to be logged for the event page not terminated on idle"
    );
    const toolbox = getToolbox(devtoolsWindow);
    const webconsole = await toolbox.selectTool("webconsole");
    const { hud } = webconsole;
    const expectedWarning =
      "Background event page was not terminated on idle because a DevTools toolbox is attached to the extension.";
    let consoleElements;
    await waitUntil(() => {
      consoleElements = findMessagesByType(hud, expectedWarning, ".warn");
      return !!consoleElements.length;
    });

    const locationElement = consoleElements[0].querySelector(
      ".frame-link-filename"
    );
    ok(
      locationElement.textContent.endsWith("_generated_background_page.html"),
      "The warning message is associated to the event page url"
    );

    info(
      "Verify event page is terminated on idle after closing the DevTools toolbox"
    );

    await closeAboutDevtoolsToolbox(document, devtoolsTab, window);
    await triggerExtensionEventPageIdleTimeout(EXTENSION_ID);
    await waitForBGStatusUpdate;
    await assertBackgroundStatus(EXTENSION_NAME, {
      document,
      expectedStatus: "stopped",
    });

    // Uninstall the test extensions.
    info("Unload extension and remove about:debugging tab");
    await AddonManager.getAddonByID(EXTENSION_ID).then(addon =>
      addon.uninstall()
    );
    info("Wait until the debug targets with test extensions disappears");
    await waitUntil(() => !findDebugTargetByText(EXTENSION_NAME, document));
    await removeTab(tab);
  }
);
