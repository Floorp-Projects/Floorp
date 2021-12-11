/* eslint-disable mozilla/no-arbitrary-setTimeout */
"use strict";
const TEST_PAGE =
  "http://mochi.test:8888/browser/browser/base/content/test/general/file_double_close_tab.html";
var testTab;

const CONTENT_PROMPT_SUBDIALOG = Services.prefs.getBoolPref(
  "prompts.contentPromptSubDialog",
  false
);

function waitForDialog(callback) {
  function onDialogLoaded(nodeOrDialogWindow) {
    let node = CONTENT_PROMPT_SUBDIALOG
      ? nodeOrDialogWindow.document.querySelector("dialog")
      : nodeOrDialogWindow;
    Services.obs.removeObserver(onDialogLoaded, "tabmodal-dialog-loaded");
    Services.obs.removeObserver(onDialogLoaded, "common-dialog-loaded");
    // Allow dialog's onLoad call to run to completion
    Promise.resolve().then(() => callback(node));
  }

  // Listen for the dialog being created
  Services.obs.addObserver(onDialogLoaded, "tabmodal-dialog-loaded");
  Services.obs.addObserver(onDialogLoaded, "common-dialog-loaded");
}

function waitForDialogDestroyed(node, callback) {
  // Now listen for the dialog going away again...
  let observer = new MutationObserver(function(muts) {
    if (!node.parentNode) {
      ok(true, "Dialog is gone");
      done();
    }
  });
  observer.observe(node.parentNode, { childList: true });

  if (CONTENT_PROMPT_SUBDIALOG) {
    node.ownerGlobal.addEventListener("unload", done);
  }

  let failureTimeout = setTimeout(function() {
    ok(false, "Dialog should have been destroyed");
    done();
  }, 10000);

  function done() {
    clearTimeout(failureTimeout);
    observer.disconnect();
    observer = null;

    if (CONTENT_PROMPT_SUBDIALOG) {
      node.ownerGlobal.removeEventListener("unload", done);
      SimpleTest.executeSoon(callback);
    } else {
      callback();
    }
  }
}

add_task(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.require_user_interaction_for_beforeunload", false]],
  });

  testTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_PAGE);

  // XXXgijs the reason this has nesting and callbacks rather than promises is
  // that DOM promises resolve on the next tick. So they're scheduled
  // in an event queue. So when we spin a new event queue for a modal dialog...
  // everything gets messed up and the promise's .then callbacks never get
  // called, despite resolve() being called just fine.
  await new Promise(resolveOuter => {
    waitForDialog(dialogNode => {
      waitForDialogDestroyed(dialogNode, () => {
        let doCompletion = () => setTimeout(resolveOuter, 0);
        info("Now checking if dialog is destroyed");

        if (CONTENT_PROMPT_SUBDIALOG) {
          ok(
            !dialogNode.ownerGlobal || dialogNode.ownerGlobal.closed,
            "onbeforeunload dialog should be gone."
          );
          if (dialogNode.ownerGlobal && !dialogNode.ownerGlobal.closed) {
            dialogNode.acceptDialog();
          }
        } else {
          ok(!dialogNode.parentNode, "onbeforeunload dialog should be gone.");
          if (dialogNode.parentNode) {
            // Failed to remove onbeforeunload dialog, so do it ourselves:
            let leaveBtn = dialogNode.querySelector(".tabmodalprompt-button0");
            waitForDialogDestroyed(dialogNode, doCompletion);
            EventUtils.synthesizeMouseAtCenter(leaveBtn, {});
            return;
          }
        }

        doCompletion();
      });
      // Click again:
      testTab.closeButton.click();
    });
    // Click once:
    testTab.closeButton.click();
  });
  await TestUtils.waitForCondition(() => !testTab.parentNode);
  ok(!testTab.parentNode, "Tab should be closed completely");
});

registerCleanupFunction(async function() {
  if (testTab.parentNode) {
    // Remove the handler, or closing this tab will prove tricky:
    try {
      await SpecialPowers.spawn(testTab.linkedBrowser, [], function() {
        content.window.onbeforeunload = null;
      });
    } catch (ex) {}
    gBrowser.removeTab(testTab);
  }
});
