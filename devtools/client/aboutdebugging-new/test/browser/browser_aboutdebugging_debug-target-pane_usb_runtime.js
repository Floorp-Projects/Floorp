/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const RUNTIME_ID = "test-runtime-id";
const RUNTIME_DEVICE_NAME = "test device name";
const RUNTIME_APP_NAME = "TestApp";

/* import-globals-from mocks/head-usb-mocks.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "mocks/head-usb-mocks.js", this);

// Test that the expected supported categories are displayed for USB runtimes.
add_task(async function() {
  const usbMocks = new UsbMocks();
  usbMocks.enableMocks();
  registerCleanupFunction(() => {
    usbMocks.disableMocks();
  });

  const { document, tab } = await openAboutDebugging();

  usbMocks.createRuntime(RUNTIME_ID, {
    deviceName: RUNTIME_DEVICE_NAME,
    name: RUNTIME_APP_NAME,
  });
  usbMocks.emitUpdate();

  await connectToRuntime(RUNTIME_DEVICE_NAME, document);
  await selectRuntime(RUNTIME_DEVICE_NAME, RUNTIME_APP_NAME, document);

  const SUPPORTED_TARGET_PANES = [
    "Extensions",
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
  usbMocks.removeRuntime(RUNTIME_ID);
  usbMocks.emitUpdate();

  info("Wait until the USB sidebar item disappears");
  await waitUntil(() => !findSidebarItemByText(RUNTIME_DEVICE_NAME, document));

  await removeTab(tab);
});
