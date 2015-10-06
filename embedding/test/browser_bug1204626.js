"use strict"; // -*- js-indent-level: 2; indent-tabs-mode: nil -*-
var Cc = Components.classes;
var Ci = Components.interfaces;
const contentBase = "https://example.com/browser/embedding/test/";
const chromeBase = "chrome://mochitests/content/browser/embedding/test/";
const testPageURL = contentBase + "bug1204626_doc0.html";

function one_test(delay, continuation) {
  let delayStr = delay === null ? "no delay" : "delay = " + delay + "ms";
  let browser;

  BrowserTestUtils.openNewForegroundTab(gBrowser, testPageURL).then((tab) => {
    browser = tab.linkedBrowser;
    let persistable = browser.QueryInterface(Ci.nsIFrameLoaderOwner)
                             .frameLoader
                             .QueryInterface(Ci.nsIWebBrowserPersistable);
    persistable.startPersistence(/* outer window ID: */ 0, {
      onDocumentReady,
      onError: function(status) {
        ok(false, new Components.Exception("startPersistence failed", status));
        continuation();
      }
    });
  });

  function onDocumentReady(doc) {
    const nameStem="test_bug1204626_" + Date.now();
    let wbp = Cc["@mozilla.org/embedding/browser/nsWebBrowserPersist;1"]
              .createInstance(Ci.nsIWebBrowserPersist);
    let tmp = Cc["@mozilla.org/file/directory_service;1"]
              .getService(Ci.nsIProperties)
              .get("TmpD", Ci.nsIFile);
    let tmpFile = tmp.clone();
    tmpFile.append(nameStem + "_saved.html");
    let tmpDir = tmp.clone();
    tmpDir.append(nameStem + "_files");

    registerCleanupFunction(function cleanUp() {
      if (tmpFile.exists()) {
        tmpFile.remove(/* recursive: */ false);
      }
      if (tmpDir.exists()) {
        tmpDir.remove(/* recursive: */ true);
      }
    });

    wbp.progressListener = {
      onProgressChange: function(){},
      onLocationChange: function(){},
      onStatusChange: function(){},
      onSecurityChange: function(){},
      onStateChange: function wbp_stateChange(_wbp, _req, state, _status) {
        if ((state & Ci.nsIWebProgressListener.STATE_STOP) == 0) {
          return;
        }
        ok(true, "Finished save (" + delayStr + ") but might have crashed.");
        continuation();
      }
    }

    function doSave() {
      wbp.saveDocument(doc, tmpFile, tmpDir, null, 0, 0);
    }
    if (delay === null) {
      doSave();
    } else {
      setTimeout(doSave, delay);
    }
    browser.messageManager.loadFrameScript("data:,content.window.close()", true);
  }
}

function test() {
  waitForExplicitFinish();
  // 0ms breaks having the actor under PBrowser, but not 10ms.
  // 10ms provokes the double-__delete__, but not 0ms.
  // And a few others, just in case.
  const testRuns = [null, 0, 10, 0, 10, 20, 50, 100];
  let i = 0;
  (function next_test() {
    if (i < testRuns.length) {
      one_test(testRuns[i++], next_test);
    } else {
      finish();
    }
  })();
}
