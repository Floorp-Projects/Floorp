/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper-collapsibilities.js */
Services.scriptloader.loadSubScript(
  CHROME_URL_ROOT + "helper-collapsibilities.js",
  this
);

const MAIN_PROCESS_NAME = "Main Process";
const MAIN_PROCESS_DESCRIPTION = "Main Process for the target browser";
const MULTIPROCESS_TOOLBOX_NAME = "Multiprocess Toolbox";
const MULTIPROCESS_TOOLBOX_DESCRIPTION =
  "Main Process and Content Processes for the target browser";

const RUNTIME_ID = "test-runtime-id";
const RUNTIME_DEVICE_NAME = "test device name";
const RUNTIME_APP_NAME = "TestApp";

// Test for main process.
add_task(async function() {
  info("Test the regular main process target");
  await pushPref("devtools.browsertoolbox.fission", false);
  await testMainProcessTarget(MAIN_PROCESS_NAME, MAIN_PROCESS_DESCRIPTION);

  info("Test the main process target with devtools.browsertoolbox.fission");
  await pushPref("devtools.browsertoolbox.fission", true);
  await testMainProcessTarget(
    MULTIPROCESS_TOOLBOX_NAME,
    MULTIPROCESS_TOOLBOX_DESCRIPTION
  );
});

async function testMainProcessTarget(name, description) {
  await pushPref("devtools.aboutdebugging.process-debugging", true);
  const mocks = new Mocks();

  const { document, tab, window } = await openAboutDebugging();

  const usbRuntime = mocks.createUSBRuntime(RUNTIME_ID, {
    deviceName: RUNTIME_DEVICE_NAME,
    name: RUNTIME_APP_NAME,
  });

  // Note: about:debugging assumes that the main process has the id 0 and will
  // rely on it to create the about:devtools-toolbox URL.
  // Only check that about:debugging doesn't create a target unnecessarily.
  usbRuntime.getMainProcess = () => {
    return {
      getTarget: () => {
        ok(false, "about:debugging should not create the main process target");
      },
    };
  };
  mocks.emitUSBUpdate();

  info("Select USB runtime");
  await connectToRuntime(RUNTIME_DEVICE_NAME, document);
  await selectRuntime(RUNTIME_DEVICE_NAME, RUNTIME_APP_NAME, document);

  info("Check debug target item of the main process");
  await waitUntil(() => findDebugTargetByText(name, document));
  const mainProcessItem = findDebugTargetByText(name, document);
  ok(mainProcessItem, "Debug target item of the main process should display");
  ok(
    mainProcessItem.textContent.includes(description),
    "Debug target item of the main process should contains the description"
  );

  info("Inspect main process");
  const { devtoolsTab, devtoolsWindow } = await openAboutDevtoolsToolbox(
    document,
    tab,
    window,
    name,
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
}
