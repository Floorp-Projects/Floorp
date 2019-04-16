/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper-collapsibilities.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "helper-collapsibilities.js", this);

const RUNTIME_ID = "test-runtime-id";
const RUNTIME_DEVICE_NAME = "test device name";
const RUNTIME_APP_NAME = "TestApp";

// Test that the expected supported categories are displayed for USB runtimes.
add_task(async function() {
  const mocks = new Mocks();
  await checkTargetPanes({ enableLocalTabs: false }, mocks);

  info("Check that enableLocalTabs has no impact on the categories displayed for remote" +
    " runtimes.");
  await checkTargetPanes({ enableLocalTabs: true }, mocks);
});

async function checkTargetPanes({ enableLocalTabs }, mocks) {
  const { document, tab, window } = await openAboutDebugging({ enableLocalTabs });
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  mocks.createUSBRuntime(RUNTIME_ID, {
    deviceName: RUNTIME_DEVICE_NAME,
    name: RUNTIME_APP_NAME,
  });
  mocks.emitUSBUpdate();

  await connectToRuntime(RUNTIME_DEVICE_NAME, document);
  await selectRuntime(RUNTIME_DEVICE_NAME, RUNTIME_APP_NAME, document);

  const SUPPORTED_TARGET_PANES = [
    "Extensions",
    "Other Workers",
    "Shared Workers",
    "Service Workers",
    "Tabs",
  ];

  for (const { title } of TARGET_PANES) {
    const debugTargetPaneEl = getDebugTargetPane(title, document);
    if (SUPPORTED_TARGET_PANES.includes(title)) {
      ok(debugTargetPaneEl, `Supported target pane [${title}] is displayed`);
    } else {
      ok(!debugTargetPaneEl, `Unsupported target pane [${title}] is hidden`);
    }
  }

  info("Remove USB runtime");
  mocks.removeUSBRuntime(RUNTIME_ID);
  mocks.emitUSBUpdate();
  await waitUntilUsbDeviceIsUnplugged(RUNTIME_DEVICE_NAME, document);

  await removeTab(tab);
}
