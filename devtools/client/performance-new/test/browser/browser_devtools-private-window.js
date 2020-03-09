/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function test() {
  info("Test opening a private browsing window while the profiler is active.");

  await withDevToolsPanel(async document => {
    const getRecordingState = setupGetRecordingState(document);

    const startRecording = await getActiveButtonFromText(
      document,
      "Start recording"
    );

    ok(!startRecording.disabled, "The start recording button is not disabled.");
    is(
      getRecordingState(),
      "available-to-record",
      "The panel is available to record."
    );

    const privateWindow = await BrowserTestUtils.openNewBrowserWindow({
      private: true,
    });

    await getElementFromDocumentByText(
      document,
      "The profiler is disabled when Private Browsing is enabled"
    );

    ok(
      startRecording.disabled,
      "The start recording button is disabled when a private browsing window is open."
    );

    is(
      getRecordingState(),
      "locked-by-private-browsing",
      "The client knows about the private window."
    );

    info("Closing the private window");
    await BrowserTestUtils.closeWindow(privateWindow);

    info("Finally wait for the start recording button to become active again.");
    await getActiveButtonFromText(document, "Start recording");

    is(
      getRecordingState(),
      "available-to-record",
      "The panel is available to record again."
    );
  });
});
