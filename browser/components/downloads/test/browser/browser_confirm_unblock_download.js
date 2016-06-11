/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the dialog which allows the user to unblock a downloaded file.

registerCleanupFunction(() => {});

function* assertDialogResult({ args, buttonToClick, expectedResult }) {
  promiseAlertDialogOpen(buttonToClick);
  is(yield DownloadsCommon.confirmUnblockDownload(args), expectedResult);
}

/**
 * Tests the "unblock" dialog, for each of the possible verdicts.
 */
add_task(function* test_unblock_dialog_unblock() {
  for (let verdict of [Downloads.Error.BLOCK_VERDICT_MALWARE,
                       Downloads.Error.BLOCK_VERDICT_POTENTIALLY_UNWANTED,
                       Downloads.Error.BLOCK_VERDICT_UNCOMMON]) {
    let args = { verdict, window, dialogType: "unblock" };

    // Test both buttons.
    yield assertDialogResult({
      args,
      buttonToClick: "accept",
      expectedResult: "unblock",
    });
    yield assertDialogResult({
      args,
      buttonToClick: "cancel",
      expectedResult: "cancel",
    });
  }
});

/**
 * Tests the "chooseUnblock" dialog for potentially unwanted downloads.
 */
add_task(function* test_chooseUnblock_dialog() {
  let args = {
    verdict: Downloads.Error.BLOCK_VERDICT_POTENTIALLY_UNWANTED,
    window,
    dialogType: "chooseUnblock",
  };

  // Test each of the three buttons.
  yield assertDialogResult({
    args,
    buttonToClick: "accept",
    expectedResult: "unblock",
  });
  yield assertDialogResult({
    args,
    buttonToClick: "cancel",
    expectedResult: "cancel",
  });
  yield assertDialogResult({
    args,
    buttonToClick: "extra1",
    expectedResult: "confirmBlock",
  });
});

/**
 * Tests the "chooseOpen" dialog for uncommon downloads.
 */
add_task(function* test_chooseOpen_dialog() {
  let args = {
    verdict: Downloads.Error.BLOCK_VERDICT_UNCOMMON,
    window,
    dialogType: "chooseOpen",
  };

  // Test each of the three buttons.
  yield assertDialogResult({
    args,
    buttonToClick: "accept",
    expectedResult: "open",
  });
  yield assertDialogResult({
    args,
    buttonToClick: "cancel",
    expectedResult: "cancel",
  });
  yield assertDialogResult({
    args,
    buttonToClick: "extra1",
    expectedResult: "confirmBlock",
  });
});
