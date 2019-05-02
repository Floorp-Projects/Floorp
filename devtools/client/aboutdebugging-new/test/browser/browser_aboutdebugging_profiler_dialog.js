/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test opening and closing the profiler dialog.
 */
add_task(async function() {
  // enable USB devices mocks
  const mocks = new Mocks();
  const usbClient = mocks.createUSBRuntime("1337id", {
    deviceName: "Fancy Phone",
    name: "Lorem ipsum",
  });

  const { document, tab, window } = await openAboutDebugging();

  mocks.emitUSBUpdate();
  await connectToRuntime("Fancy Phone", document);
  await selectRuntime("Fancy Phone", "Lorem ipsum", document);

  info("Open the profiler dialog");
  await openProfilerDialog(usbClient, document);
  assertDialogVisible(document);

  info("Click on the close button and wait until the dialog disappears");
  const closeDialogButton = document.querySelector(".qa-profiler-dialog-close");
  closeDialogButton.click();
  await waitUntil(() => !document.querySelector(".qa-profiler-dialog"));
  assertDialogHidden(document);

  info("Open the profiler dialog again");
  await openProfilerDialog(usbClient, document);
  assertDialogVisible(document);

  info("Click on the mask element and wait until the dialog disappears");
  const mask = document.querySelector(".qa-profiler-dialog-mask");
  EventUtils.synthesizeMouse(mask, 5, 5, {}, window);
  await waitUntil(() => !document.querySelector(".qa-profiler-dialog"));
  assertDialogHidden(document);

  info("Open the profiler dialog again");
  await openProfilerDialog(usbClient, document);
  assertDialogVisible(document);

  info("Navigate to this-firefox and wait until the dialog disappears");
  document.location.hash = "#/runtime/this-firefox";
  await waitUntil(() => !document.querySelector(".qa-profiler-dialog"));
  assertDialogHidden(document);

  info("Select the remote runtime again, check the dialog is still hidden");
  await selectRuntime("Fancy Phone", "Lorem ipsum", document);
  assertDialogHidden(document);

  await removeTab(tab);
});

function assertDialogVisible(doc) {
  ok(doc.querySelector(".qa-profiler-dialog"), "Dialog is displayed");
  ok(doc.querySelector(".qa-profiler-dialog-mask"), "Dialog mask is displayed");
}

function assertDialogHidden(doc) {
  ok(!document.querySelector(".qa-profiler-dialog"), "Dialog is removed");
  ok(!document.querySelector(".qa-profiler-dialog-mask"), "Dialog mask is removed");
}
