/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

// Test basic recording of a tab without any debugging.
add_task(async function() {
  const recordingTab = await openRecordingTab("doc_rr_basic.html");
  await once(Services.ppmm, "RecordingFinished");

  await gBrowser.removeTab(recordingTab);

  ok(true, "Finished");
});
