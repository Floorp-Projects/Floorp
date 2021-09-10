/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const RUNTIME_ID = "1337id";
const DEVICE_NAME = "Fancy Phone";
const RUNTIME_NAME = "Lorem ipsum";

/**
 * Test opening and closing the profiler dialog.
 */
add_task(async function() {
  // This is a client to the current Firefox.
  const clientWrapper = await createLocalClientWrapper();

  // enable USB devices mocks
  const mocks = new Mocks();
  mocks.createUSBRuntime(RUNTIME_ID, {
    deviceName: DEVICE_NAME,
    name: RUNTIME_NAME,
    clientWrapper,
  });

  const { document, tab, window } = await openAboutDebugging();

  mocks.emitUSBUpdate();
  await connectToRuntime(DEVICE_NAME, document);
  await selectRuntime(DEVICE_NAME, RUNTIME_NAME, document);

  info("Open the profiler dialog");
  await openProfilerDialogWithRealClient(document);
  assertDialogVisible(document);

  info("Click on the close button and wait until the dialog disappears");
  const closeDialogButton = document.querySelector(".qa-profiler-dialog-close");
  closeDialogButton.click();
  await waitUntil(() => !document.querySelector(".qa-profiler-dialog"));
  assertDialogHidden(document);

  info("Open the profiler dialog again");
  await openProfilerDialogWithRealClient(document);
  assertDialogVisible(document);

  info("Click on the mask element and wait until the dialog disappears");
  const mask = document.querySelector(".qa-profiler-dialog-mask");
  EventUtils.synthesizeMouse(mask, 5, 5, {}, window);
  await waitUntil(() => !document.querySelector(".qa-profiler-dialog"));
  assertDialogHidden(document);

  info("Open the profiler dialog again");
  await openProfilerDialogWithRealClient(document);
  assertDialogVisible(document);

  info("Navigate to this-firefox and wait until the dialog disappears");
  document.location.hash = "#/runtime/this-firefox";
  await waitUntil(() => !document.querySelector(".qa-profiler-dialog"));
  assertDialogHidden(document);

  info("Select the remote runtime again, check the dialog is still hidden");
  await selectRuntime(DEVICE_NAME, RUNTIME_NAME, document);
  assertDialogHidden(document);

  info("Remove USB runtime");
  mocks.removeUSBRuntime(RUNTIME_ID);
  mocks.emitUSBUpdate();
  await waitUntilUsbDeviceIsUnplugged(DEVICE_NAME, document);
  await removeTab(tab);
});

function assertDialogVisible(doc) {
  ok(doc.querySelector(".qa-profiler-dialog"), "Dialog is displayed");
  ok(doc.querySelector(".qa-profiler-dialog-mask"), "Dialog mask is displayed");
}

function assertDialogHidden(doc) {
  ok(!doc.querySelector(".qa-profiler-dialog"), "Dialog is removed");
  ok(!doc.querySelector(".qa-profiler-dialog-mask"), "Dialog mask is removed");
}

/**
 * Open the performance profiler dialog with a real client.
 */
async function openProfilerDialogWithRealClient(doc) {
  info("Click on the Profile Runtime button");
  const profileButton = doc.querySelector(".qa-profile-runtime-button");
  profileButton.click();

  info("Wait for the rendering of the profiler UI");
  const profilerIframe = await TestUtils.waitForCondition(
    () => doc.querySelector(".profiler-dialog__frame"),
    "Waiting for the profiler frame."
  );
  await TestUtils.waitForCondition(
    () =>
      profilerIframe.contentDocument &&
      profilerIframe.contentDocument.querySelector(".perf-presets"),
    "Waiting for the rendering of the profiler UI."
  );
  return profilerIframe.contentDocument;
}
