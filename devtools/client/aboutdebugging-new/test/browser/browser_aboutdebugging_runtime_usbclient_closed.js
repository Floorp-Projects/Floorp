/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper-mocks.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "helper-mocks.js", this);

const RUNTIME_DEVICE_ID = "1234";
const RUNTIME_DEVICE_NAME = "A device";
const RUNTIME_NAME = "Test application";

// Test that about:debugging navigates back to the default page when the server for the
// current USB runtime is closed.
add_task(async function() {
  const mocks = new Mocks();

  const { document, tab } = await openAboutDebugging();

  const usbClient = mocks.createUSBRuntime(RUNTIME_DEVICE_ID, {
    deviceName: RUNTIME_DEVICE_NAME,
    name: RUNTIME_NAME,
  });
  mocks.emitUSBUpdate();

  info("Connect to and select the USB device");
  await connectToRuntime(RUNTIME_DEVICE_NAME, document);
  await selectRuntime(RUNTIME_DEVICE_NAME, RUNTIME_NAME, document);

  info("Simulate a client disconnection");
  mocks.removeUSBRuntime(RUNTIME_DEVICE_ID);
  usbClient._eventEmitter.emit("closed");

  info("Wait until the sidebar item for this runtime disappears");
  await waitUntil(() => !findSidebarItemByText(RUNTIME_DEVICE_NAME, document));

  is(document.location.hash, `#/runtime/this-firefox`,
    "Redirection to the default page (this-firefox)");

  await removeTab(tab);
});
