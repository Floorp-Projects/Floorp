/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that the about:debugging document and the profiler dialog document
// use the expected document direction.
add_task(async function test_direction_is_ltr_by_default() {
  await testAboutDebuggingDocsDirection("ltr");
});

add_task(async function test_direction_is_rtl_for_bidi_pseudolocale() {
  await pushPref("intl.l10n.pseudo", "bidi");
  await testAboutDebuggingDocsDirection("rtl");
});

async function testAboutDebuggingDocsDirection(expectedDir) {
  const mocks = new Mocks();
  const { document, usbClient } = await setupTestForMockUSBRuntime(mocks);

  is(document.dir, expectedDir, "document dir is " + expectedDir);

  info("Open the profiler dialog");
  await openProfilerDialog(usbClient, document);

  const profilerDialogFrame = document.querySelector(
    ".qa-profiler-dialog iframe"
  );
  ok(profilerDialogFrame, "Found Profiler dialog iframe");

  const profilerDoc = profilerDialogFrame.contentWindow.document;
  is(profilerDoc.dir, expectedDir, "Profiler document dir is " + expectedDir);

  await teardownTestForMockUSBRuntime(mocks, document);
}

async function setupTestForMockUSBRuntime(mocks) {
  info("Setup mock USB runtime");

  const usbClient = mocks.createUSBRuntime("runtimeId", {
    deviceName: "deviceName",
    name: "runtimeName",
  });

  info("Open about:debugging and select runtime page for mock USB runtime");
  const { document } = await openAboutDebugging();

  mocks.emitUSBUpdate();
  await connectToRuntime("deviceName", document);
  await selectRuntime("deviceName", "runtimeName", document);

  return { document, usbClient };
}

async function teardownTestForMockUSBRuntime(mocks, doc) {
  info("Remove mock USB runtime");

  mocks.removeUSBRuntime("runtimeId");
  mocks.emitUSBUpdate();
  await waitUntilUsbDeviceIsUnplugged("deviceName", doc);
}
