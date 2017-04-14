"use strict";
const TEST_PAGE = "http://mochi.test:8888/browser/browser/base/content/test/general/file_double_close_tab.html";
var testTab;

SpecialPowers.pushPrefEnv({"set": [["dom.require_user_interaction_for_beforeunload", false]]});

function waitForDialog(callback) {
  function onTabModalDialogLoaded(node) {
    Services.obs.removeObserver(onTabModalDialogLoaded, "tabmodal-dialog-loaded");
    callback(node);
  }

  // Listen for the dialog being created
  Services.obs.addObserver(onTabModalDialogLoaded, "tabmodal-dialog-loaded");
}

function waitForDialogDestroyed(node, callback) {
  // Now listen for the dialog going away again...
  let observer = new MutationObserver(function(muts) {
    if (!node.parentNode) {
      ok(true, "Dialog is gone");
      done();
    }
  });
  observer.observe(node.parentNode, {childList: true});
  let failureTimeout = setTimeout(function() {
    ok(false, "Dialog should have been destroyed");
    done();
  }, 10000);

  function done() {
    clearTimeout(failureTimeout);
    observer.disconnect();
    observer = null;
    callback();
  }
}

add_task(function*() {
  testTab = gBrowser.selectedTab = gBrowser.addTab();
  yield promiseTabLoadEvent(testTab, TEST_PAGE);
  // XXXgijs the reason this has nesting and callbacks rather than promises is
  // that DOM promises resolve on the next tick. So they're scheduled
  // in an event queue. So when we spin a new event queue for a modal dialog...
  // everything gets messed up and the promise's .then callbacks never get
  // called, despite resolve() being called just fine.
  yield new Promise(resolveOuter => {
    waitForDialog(dialogNode => {
      waitForDialogDestroyed(dialogNode, () => {
        let doCompletion = () => setTimeout(resolveOuter, 0);
        info("Now checking if dialog is destroyed");
        ok(!dialogNode.parentNode, "onbeforeunload dialog should be gone.");
        if (dialogNode.parentNode) {
          // Failed to remove onbeforeunload dialog, so do it ourselves:
          let leaveBtn = dialogNode.ui.button0;
          waitForDialogDestroyed(dialogNode, doCompletion);
          EventUtils.synthesizeMouseAtCenter(leaveBtn, {});
          return;
        }
        doCompletion();
      });
      // Click again:
      document.getAnonymousElementByAttribute(testTab, "anonid", "close-button").click();
    });
    // Click once:
    document.getAnonymousElementByAttribute(testTab, "anonid", "close-button").click();
  });
  yield promiseWaitForCondition(() => !testTab.parentNode);
  ok(!testTab.parentNode, "Tab should be closed completely");
});

registerCleanupFunction(function() {
  if (testTab.parentNode) {
    // Remove the handler, or closing this tab will prove tricky:
    try {
      testTab.linkedBrowser.contentWindow.onbeforeunload = null;
    } catch (ex) {}
    gBrowser.removeTab(testTab);
  }
});
