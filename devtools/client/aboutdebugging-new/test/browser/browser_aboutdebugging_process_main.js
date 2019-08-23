/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper-collapsibilities.js */
Services.scriptloader.loadSubScript(
  CHROME_URL_ROOT + "helper-collapsibilities.js",
  this
);

const MAIN_PROCESS_NAME = "Main Process";
const RUNTIME_ID = "test-runtime-id";
const RUNTIME_DEVICE_NAME = "test device name";
const RUNTIME_APP_NAME = "TestApp";

// Test for main process.
add_task(async function() {
  await pushPref("devtools.aboutdebugging.process-debugging", true);

  const mocks = new Mocks();

  const { document, tab, window } = await openAboutDebugging();

  const usbRuntime = mocks.createUSBRuntime(RUNTIME_ID, {
    deviceName: RUNTIME_DEVICE_NAME,
    name: RUNTIME_APP_NAME,
  });
  usbRuntime.getMainProcess = () => {
    return { actorID: 0 };
  };
  mocks.emitUSBUpdate();

  info("Select USB runtime");
  await connectToRuntime(RUNTIME_DEVICE_NAME, document);
  await selectRuntime(RUNTIME_DEVICE_NAME, RUNTIME_APP_NAME, document);

  info("Check debug target item of the main process");
  const mainProcessItem = findDebugTargetByText(MAIN_PROCESS_NAME, document);
  ok(mainProcessItem, "Debug target item of the main process should display");
  ok(
    mainProcessItem.textContent.includes("Main Process for the target browser"),
    "Debug target item of the main process should contains the description"
  );

  info("Inspect main process");
  const { devtoolsTab, devtoolsWindow } = await openAboutDevtoolsToolbox(
    document,
    tab,
    window,
    MAIN_PROCESS_NAME,
    false
  );

  const url = new window.URL(devtoolsWindow.location.href);
  const processID = url.searchParams.get("id");
  is(processID, "0", "Correct process id");
  const remoteID = url.searchParams.get("remoteId");
  is(remoteID, `${RUNTIME_ID}-usb`, "Correct remote runtime id");

  info("Remove USB runtime");
  mocks.removeUSBRuntime(RUNTIME_ID);
  mocks.emitUSBUpdate();
  await waitUntilUsbDeviceIsUnplugged(RUNTIME_DEVICE_NAME, document);

  // Note that we can't use `closeAboutDevtoolsToolbox` because the toolbox init
  // is expected to fail, and we are redirected to the error page.
  await removeTab(devtoolsTab);
  await waitUntil(() => !findDebugTargetByText("Toolbox - ", document));
  await removeTab(tab);
});
