/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const {Services} = Cu.import("resource://gre/modules/Services.jsm", {});

const HISTOGRAM_NAME = "FX_SESSION_RESTORE_SEND_UPDATE_CAUSED_OOM";

/**
 * Test that an OOM in sendAsyncMessage in a framescript will be reported
 * to Telemetry.
 */

add_task(async function init() {
  Services.telemetry.canRecordExtended = true;
});

function frameScript() {
  // Make send[A]syncMessage("SessionStore:update", ...) simulate OOM.
  // Other operations are unaffected.
  let mm = docShell.sameTypeRootTreeItem.
    QueryInterface(Ci.nsIDocShell).
    QueryInterface(Ci.nsIInterfaceRequestor).
    getInterface(Ci.nsIContentFrameMessageManager);

  let wrap = function(original) {
    return function(name, ...args) {
      if (name != "SessionStore:update") {
        return original(name, ...args);
      }
      throw new Components.Exception("Simulated OOM", Cr.NS_ERROR_OUT_OF_MEMORY);
    }
  }

  mm.sendAsyncMessage = wrap(mm.sendAsyncMessage);
  mm.sendSyncMessage = wrap(mm.sendSyncMessage);
}

add_task(async function() {
  // Capture original state.
  let snapshot = Services.telemetry.getHistogramById(HISTOGRAM_NAME).snapshot();

  // Open a browser, configure it to cause OOM.
  let newTab = BrowserTestUtils.addTab(gBrowser, "about:robots");
  let browser = newTab.linkedBrowser;
  await ContentTask.spawn(browser, null, frameScript);


  let promiseReported = new Promise(resolve => {
    browser.messageManager.addMessageListener("SessionStore:error", resolve);
  });

  // Attempt to flush. This should fail.
  let promiseFlushed = TabStateFlusher.flush(browser);
  promiseFlushed.then((success) => {
    if (success) {
      throw new Error("Flush should have failed")
    }
  });

  // The frame script should report an error.
  await promiseReported;

  // Give us some time to handle that error.
  await new Promise(resolve => setTimeout(resolve, 10));

  // By now, Telemetry should have been updated.
  let snapshot2 = Services.telemetry.getHistogramById(HISTOGRAM_NAME).snapshot();
  gBrowser.removeTab(newTab);

  Assert.ok(snapshot2.sum > snapshot.sum);
});

add_task(async function cleanup() {
  Services.telemetry.canRecordExtended = false;
});
