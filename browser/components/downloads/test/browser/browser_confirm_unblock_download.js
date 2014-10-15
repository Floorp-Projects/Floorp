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

add_task(function* test_confirm_unblock_dialog_unblock() {
  addDialogOpenObserver("cancel");
  let result = yield DownloadsCommon.confirmUnblockDownload(DownloadsCommon.UNBLOCK_MALWARE,
                                                            window);
  ok(result, "Should return true when the user clicks on `Unblock` button.");
});

add_task(function* test_confirm_unblock_dialog_keep_safe() {
  addDialogOpenObserver("accept");
  let result = yield DownloadsCommon.confirmUnblockDownload(DownloadsCommon.UNBLOCK_MALWARE,
                                                            window);
  ok(!result, "Should return false when the user clicks on `Keep me safe` button.");
});
