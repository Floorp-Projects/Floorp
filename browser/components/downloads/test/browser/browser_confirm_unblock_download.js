/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the dialog which allows the user to unblock a downloaded file.

registerCleanupFunction(() => {});

async function assertDialogResult({ args, buttonToClick, expectedResult }) {
  let promise = BrowserTestUtils.promiseAlertDialog(buttonToClick);
  is(
    await DownloadsCommon.confirmUnblockDownload(args),
    expectedResult,
    `Expect ${expectedResult} from ${buttonToClick}`
  );
  await promise;
}

/**
 * Tests the "unblock" dialog, for each of the possible verdicts.
 */
add_task(async function test_unblock_dialog_unblock() {
  for (let verdict of [
    Downloads.Error.BLOCK_VERDICT_MALWARE,
    Downloads.Error.BLOCK_VERDICT_POTENTIALLY_UNWANTED,
    Downloads.Error.BLOCK_VERDICT_UNCOMMON,
  ]) {
    let args = { verdict, window, dialogType: "unblock" };

    // Test both buttons.
    await assertDialogResult({
      args,
      buttonToClick: "accept",
      expectedResult: "unblock",
    });
    await assertDialogResult({
      args,
      buttonToClick: "cancel",
      expectedResult: "cancel",
    });
  }
});

/**
 * Tests the "chooseUnblock" dialog for potentially unwanted downloads.
 */
add_task(async function test_chooseUnblock_dialog() {
  let args = {
    verdict: Downloads.Error.BLOCK_VERDICT_POTENTIALLY_UNWANTED,
    window,
    dialogType: "chooseUnblock",
  };

  // Test each of the three buttons.
  await assertDialogResult({
    args,
    buttonToClick: "accept",
    expectedResult: "unblock",
  });
  await assertDialogResult({
    args,
    buttonToClick: "cancel",
    expectedResult: "cancel",
  });
  await assertDialogResult({
    args,
    buttonToClick: "extra1",
    expectedResult: "confirmBlock",
  });
});

/**
 * Tests the "chooseOpen" dialog for uncommon downloads.
 */
add_task(async function test_chooseOpen_dialog() {
  let args = {
    verdict: Downloads.Error.BLOCK_VERDICT_UNCOMMON,
    window,
    dialogType: "chooseOpen",
  };

  // Test each of the three buttons.
  await assertDialogResult({
    args,
    buttonToClick: "accept",
    expectedResult: "open",
  });
  await assertDialogResult({
    args,
    buttonToClick: "cancel",
    expectedResult: "cancel",
  });
  await assertDialogResult({
    args,
    buttonToClick: "extra1",
    expectedResult: "confirmBlock",
  });
});
