/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const RUNTIME_ID = "1337id";
const DEVICE_NAME = "Fancy Phone";
const SERVER_RUNTIME_NAME = "Mozilla Firefox";
const ADB_RUNTIME_NAME = "Firefox Preview";
const SERVER_VERSION = "v7.3.31";
const ADB_VERSION = "v1.3.37";

/**
 * Check that the node picker button in about:devtools-toolbox has the expected class when
 * connecting to an Android phone.
 */
add_task(async function() {
  // We use a real local client combined with a mocked USB runtime to be able to open
  // about:devtools-toolbox on a real target.
  const clientWrapper = await createLocalClientWrapper();

  const mocks = new Mocks();
  mocks.createUSBRuntime(RUNTIME_ID, {
    channel: "nightly",
    clientWrapper,
    deviceName: DEVICE_NAME,
    isFenix: true,
    name: SERVER_RUNTIME_NAME,
    shortName: ADB_RUNTIME_NAME,
    versionName: ADB_VERSION,
    version: SERVER_VERSION,
  });

  // open a remote runtime page
  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  mocks.emitUSBUpdate();
  info("Select the runtime page for the USB runtime");
  const onRequestSuccess = waitForRequestsSuccess(window.AboutDebugging.store);
  await connectToRuntime(DEVICE_NAME, document);
  await selectRuntime(DEVICE_NAME, ADB_RUNTIME_NAME, document);
  info(
    "Wait for requests to finish the USB runtime is backed by a real local client"
  );
  await onRequestSuccess;

  info("Wait for the about:debugging target to be available");
  await waitUntil(() => findDebugTargetByText("about:debugging", document));
  const { devtoolsDocument, devtoolsTab } = await openAboutDevtoolsToolbox(
    document,
    tab,
    window
  );

  const pickerButton = devtoolsDocument.querySelector("#command-button-pick");
  ok(
    pickerButton.classList.contains("remote-fenix"),
    "The node picker does have the expected additional className when debugging an android phone"
  );
  const pickerButtonTitle = pickerButton.getAttribute("title");
  const expectedKeyboardShortcut =
    Services.appinfo.OS === "Darwin"
      ? `Cmd+Shift+C or Cmd+Opt+C`
      : `Ctrl+Shift+C`;
  is(
    pickerButtonTitle,
    `Pick an element from the Android phone (${expectedKeyboardShortcut})`,
    `The node picker does have the expected tooltip when debugging an android phone`
  );

  info("Wait for all pending requests to settle on the DevToolsClient");
  await clientWrapper.client.waitForRequestsToSettle();
  await closeAboutDevtoolsToolbox(document, devtoolsTab, window);

  info("Remove USB runtime");
  mocks.removeUSBRuntime(RUNTIME_ID);
  mocks.emitUSBUpdate();
  await waitUntilUsbDeviceIsUnplugged(DEVICE_NAME, document);

  await removeTab(tab);
  await clientWrapper.close();
});
