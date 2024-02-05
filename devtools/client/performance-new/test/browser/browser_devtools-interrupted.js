/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

add_task(async function test() {
  info("Test what happens when a recording is interrupted by another tool.");

  const { stopProfiler: stopProfilerByAnotherTool } =
    ChromeUtils.importESModule(
      "resource://devtools/client/performance-new/shared/background.sys.mjs"
    );

  await withDevToolsPanel(async document => {
    const getRecordingState = setupGetRecordingState(document);

    const startRecording = await getActiveButtonFromText(
      document,
      "Start recording"
    );
    info("Click to start recording");
    startRecording.click();

    info("Wait until the profiler UI has updated to show that it is ready.");
    await getActiveButtonFromText(document, "Capture recording");

    info("Stop the profiler by another tool.");

    stopProfilerByAnotherTool();

    info("Check that the user was notified of this interruption.");
    await getElementFromDocumentByText(
      document,
      "The recording was stopped by another tool."
    );

    is(
      getRecordingState(),
      "available-to-record",
      "The client is ready to record again, even though it was interrupted."
    );
  });
});
