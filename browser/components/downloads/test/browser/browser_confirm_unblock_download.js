/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the dialog which allows the user to unblock a downloaded file.

registerCleanupFunction(() => {});

function addDialogOpenObserver(buttonAction) {
  Services.ww.registerNotification(function onOpen(subj, topic, data) {
    if (topic == "domwindowopened" && subj instanceof Ci.nsIDOMWindow) {
      // The test listens for the "load" event which guarantees that the alert
      // class has already been added (it is added when "DOMContentLoaded" is
      // fired).
      subj.addEventListener("load", function onLoad() {
        subj.removeEventListener("load", onLoad);
        if (subj.document.documentURI ==
            "chrome://global/content/commonDialog.xul") {
          Services.ww.unregisterNotification(onOpen);

          let dialog = subj.document.getElementById("commonDialog");
          ok(dialog.classList.contains("alert-dialog"),
             "The dialog element should contain an alert class.");

          let doc = subj.document.documentElement;
          doc.getButton(buttonAction).click();
        }
      });
    }
  });
}

function* assertDialogResult({ args, buttonToClick, expectedResult }) {
  addDialogOpenObserver(buttonToClick);
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
