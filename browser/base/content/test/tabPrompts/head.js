/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);

// Waits for an auth dialog to appear and closes it.
// Also checks an index of a given histopgram
async function waitForDialog(index, histogram) {
  await TestUtils.topicObserved("common-dialog-loaded");
  let dialog = gBrowser.getTabDialogBox(gBrowser.selectedBrowser)
    ._tabDialogManager._topDialog;
  let dialogDocument = dialog._frame.contentDocument;
  let onDialogClosed = BrowserTestUtils.waitForEvent(
    window,
    "DOMModalDialogClosed"
  );
  TelemetryTestUtils.assertHistogram(histogram, index, 1);
  // it does not matter if the dialog is canceled or accepted for our telemety so we just always cancel
  dialogDocument.getElementById("commonDialog").cancelDialog();

  await onDialogClosed;
}
