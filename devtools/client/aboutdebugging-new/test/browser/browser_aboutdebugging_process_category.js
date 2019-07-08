/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const CATEGORY_NAME = "Processes";
const RUNTIME_ID = "test-runtime-id";
const RUNTIME_DEVICE_NAME = "test device name";
const RUNTIME_APP_NAME = "TestApp";

// Test whether process category exists by the runtime type.
add_task(async function() {
  await pushPref("devtools.aboutdebugging.process-debugging", true);

  const mocks = new Mocks();

  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  const usbRuntime = mocks.createUSBRuntime(RUNTIME_ID, {
    deviceName: RUNTIME_DEVICE_NAME,
    name: RUNTIME_APP_NAME,
  });
  usbRuntime.getMainProcess = () => {
    return { actorID: 0 };
  };
  mocks.emitUSBUpdate();

  info("Check the process category existence for this firefox");
  ok(
    !getDebugTargetPane(CATEGORY_NAME, document),
    "Process category should not display for this firefox"
  );

  info("Check the process category existence for USB runtime");
  await connectToRuntime(RUNTIME_DEVICE_NAME, document);
  await selectRuntime(RUNTIME_DEVICE_NAME, RUNTIME_APP_NAME, document);
  ok(
    getDebugTargetPane(CATEGORY_NAME, document),
    "Process category should display for USB runtime"
  );

  info("Remove USB runtime");
  mocks.removeUSBRuntime(RUNTIME_ID);
  mocks.emitUSBUpdate();
  await waitUntilUsbDeviceIsUnplugged(RUNTIME_DEVICE_NAME, document);

  await removeTab(tab);
});
