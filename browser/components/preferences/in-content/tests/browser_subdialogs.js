/* Any copyright is dedicated to the Public Domain.
* http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests for the sub-dialog infrastructure, not for actual sub-dialog functionality.
 */

let gTeardownAfterClose = false;
const gDialogURL = getRootDirectory(gTestPath) + "subdialog.xul";

function test() {
  waitForExplicitFinish();
  open_preferences((win) => {
    Task.spawn(function () {
      for (let test of gTests) {
        info("STARTING TEST: " + test.desc);
        try {
          yield test.run();
        } finally {
          if (test.teardown) {
            yield test.teardown();
          }
        }
      }
    });
  });
}

let gTests = [{
  desc: "Check titlebar, focus, return value, title changes, and accepting",
  run: function* () {
    let rv = { acceptCount: 0 };
    let deferredClose = Promise.defer();
    let dialogPromise = openAndLoadSubDialog(gDialogURL, null, rv,
                                             (aEvent) => dialogClosingCallback(deferredClose, aEvent));
    let dialog = yield dialogPromise;

    // Check focus is in the textbox
    ise(dialog.document.activeElement.value, "Default text", "Textbox with correct text is focused");

    // Titlebar
    ise(content.document.getElementById("dialogTitle").textContent, "Sample sub-dialog",
       "Dialog title should be correct initially");
    let receivedEvent = waitForEvent(gBrowser.selectedBrowser, "DOMTitleChanged");
    dialog.document.title = "Updated title";
    // Wait for the title change listener
    yield receivedEvent;
    ise(content.document.getElementById("dialogTitle").textContent, "Updated title",
       "Dialog title should be updated with changes");

    let closingPromise = promiseDialogClosing(dialog);

    // Accept the dialog
    dialog.document.documentElement.acceptDialog();
    let closingEvent = yield closingPromise;
    ise(closingEvent.detail.button, "accept", "closing event should indicate button was 'accept'");

    yield deferredClose.promise;
    ise(rv.acceptCount, 1, "return value should have been updated");
  },
},
{
  desc: "Check canceling the dialog",
  run: function* () {
    let rv = { acceptCount: 0 };
    let deferredClose = Promise.defer();
    let dialogPromise = openAndLoadSubDialog(gDialogURL, null, rv,
                                             (aEvent) => dialogClosingCallback(deferredClose, aEvent));
    let dialog = yield dialogPromise;

    let closingPromise = promiseDialogClosing(dialog);

    info("cancelling the dialog");
    dialog.document.documentElement.cancelDialog();

    let closingEvent = yield closingPromise;
    ise(closingEvent.detail.button, "cancel", "closing event should indicate button was 'cancel'");

    yield deferredClose.promise;
    ise(rv.acceptCount, 0, "return value should NOT have been updated");
  },
},
{
  desc: "Check window.close on the dialog",
  run: function* () {
    let rv = { acceptCount: 0 };
    let deferredClose = Promise.defer();
    let dialogPromise = openAndLoadSubDialog(gDialogURL, null, rv,
                                             (aEvent) => dialogClosingCallback(deferredClose, aEvent));
    let dialog = yield dialogPromise;

    let closingPromise = promiseDialogClosing(dialog);
    info("window.close called on the dialog");
    dialog.window.close();

    let closingEvent = yield closingPromise;
    ise(closingEvent.detail.button, null, "closing event should indicate no button was clicked");

    yield deferredClose.promise;
    ise(rv.acceptCount, 0, "return value should NOT have been updated");
  },
},
{
  desc: "Check clicking the close button on the dialog",
  run: function* () {
    let rv = { acceptCount: 0 };
    let deferredClose = Promise.defer();
    let dialogPromise = openAndLoadSubDialog(gDialogURL, null, rv,
                                             (aEvent) => dialogClosingCallback(deferredClose, aEvent));
    let dialog = yield dialogPromise;

    yield EventUtils.synthesizeMouseAtCenter(content.document.getElementById("dialogClose"), {},
                                             content.window);

    yield deferredClose.promise;
    ise(rv.acceptCount, 0, "return value should NOT have been updated");
  },
},
{
  desc: "Check that 'back' navigation will close the dialog",
  run: function* () {
    let rv = { acceptCount: 0 };
    let deferredClose = Promise.defer();
    let dialogPromise = openAndLoadSubDialog(gDialogURL, null, rv,
                                             (aEvent) => dialogClosingCallback(deferredClose, aEvent));
    let dialog = yield dialogPromise;

    info("cancelling the dialog");
    content.gSubDialog._frame.goBack();

    yield deferredClose.promise;
    ise(rv.acceptCount, 0, "return value should NOT have been updated");
  },
},
{
  desc: "Hitting escape in the dialog",
  run: function* () {
    let rv = { acceptCount: 0 };
    let deferredClose = Promise.defer();
    let dialogPromise = openAndLoadSubDialog(gDialogURL, null, rv,
                                             (aEvent) => dialogClosingCallback(deferredClose, aEvent));
    let dialog = yield dialogPromise;

    EventUtils.synthesizeKey("VK_ESCAPE", {}, content.window);

    yield deferredClose.promise;
    ise(rv.acceptCount, 0, "return value should NOT have been updated");
  },
},
{
  desc: "Check that width and height from the sub-dialog are used to size the <browser>",
  run: function* () {
    let deferredClose = Promise.defer();
    let dialogPromise = openAndLoadSubDialog(gDialogURL, null, null,
                                             (aEvent) => dialogClosingCallback(deferredClose, aEvent));
    let dialog = yield dialogPromise;

    ise(content.gSubDialog._frame.style.width, "528px", "Width should be set on the frame from the dialog");
    ise(content.gSubDialog._frame.style.height, "280px", "Height should be set on the frame from the dialog");

    content.gSubDialog.close();
    yield deferredClose.promise;
  },
},
{
  desc: "Check that scrollWidth and scrollHeight from the sub-dialog are used to size the <browser>",
  run: function* () {
    let deferredClose = Promise.defer();
    let dialogPromise = openAndLoadSubDialog(gDialogURL, null, null,
                                             (aEvent) => dialogClosingCallback(deferredClose, aEvent));

    content.addEventListener("DOMFrameContentLoaded", function frameLoaded() {
      content.removeEventListener("DOMFrameContentLoaded", frameLoaded);
      content.gSubDialog._frame.contentDocument.documentElement.style.height = "";
      content.gSubDialog._frame.contentDocument.documentElement.style.width = "";
    });

    let dialog = yield dialogPromise;

    ok(content.gSubDialog._frame.style.width.endsWith("px"),
       "Width should be set to a px value of the scrollWidth from the dialog");
    ok(content.gSubDialog._frame.style.height.endsWith("px"),
       "Height should be set to a px value of the scrollHeight from the dialog");

    gTeardownAfterClose = true;
    content.gSubDialog.close();
    yield deferredClose.promise;
  },
}];

function promiseDialogClosing(dialog) {
  return waitForEvent(dialog, "dialogclosing");
}

function dialogClosingCallback(aPromise, aEvent) {
  // Wait for the close handler to unload the page
  waitForEvent(content.gSubDialog._frame, "load", 4000).then((aEvt) => {
    info("Load event happened: " + !(aEvt instanceof Error));
    is_element_hidden(content.gSubDialog._overlay, "Overlay is not visible");
    ise(content.gSubDialog._frame.getAttribute("style"), "",
        "Check that inline styles were cleared");
    ise(content.gSubDialog._frame.contentWindow.location.toString(), "about:blank",
       "Check the sub-dialog was unloaded");
    if (gTeardownAfterClose) {
      content.close();
      finish();
    }
    aPromise.resolve();
  }, Cu.reportError);
}
