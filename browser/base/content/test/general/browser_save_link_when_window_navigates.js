/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var MockFilePicker = SpecialPowers.MockFilePicker;
MockFilePicker.init(window);

const SAVE_PER_SITE_PREF = "browser.download.lastDir.savePerSite";
const ALWAYS_DOWNLOAD_DIR_PREF = "browser.download.useDownloadDir";
const UCT_URI = "chrome://mozapps/content/downloads/unknownContentType.xul";

/* import-globals-from ../../../../../toolkit/content/tests/browser/common/mockTransfer.js */
Cc["@mozilla.org/moz/jssubscript-loader;1"]
  .getService(Ci.mozIJSSubScriptLoader)
  .loadSubScript("chrome://mochitests/content/browser/toolkit/content/tests/browser/common/mockTransfer.js",
                 this);

function createTemporarySaveDirectory() {
  var saveDir = Cc["@mozilla.org/file/directory_service;1"]
                  .getService(Ci.nsIProperties)
                  .get("TmpD", Ci.nsIFile);
  saveDir.append("testsavedir");
  if (!saveDir.exists()) {
    info("create testsavedir!");
    saveDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
  }
  info("return from createTempSaveDir: " + saveDir.path);
  return saveDir;
}

function triggerSave(aWindow, aCallback) {
  info("started triggerSave, persite downloads: " + (Services.prefs.getBoolPref(SAVE_PER_SITE_PREF) ? "on" : "off"));
  var fileName;
  let testBrowser = aWindow.gBrowser.selectedBrowser;
  let testURI = "http://mochi.test:8888/browser/browser/base/content/test/general/navigating_window_with_download.html";
  windowObserver.setCallback(onUCTDialog);
  testBrowser.loadURI(testURI);

  // Create the folder the link will be saved into.
  var destDir = createTemporarySaveDirectory();
  var destFile = destDir.clone();

  MockFilePicker.displayDirectory = destDir;
  MockFilePicker.showCallback = function(fp) {
    info("showCallback");
    fileName = fp.defaultString;
    info("fileName: " + fileName);
    destFile.append(fileName);
    MockFilePicker.setFiles([destFile]);
    MockFilePicker.filterIndex = 1; // kSaveAsType_URL
    info("done showCallback");
  };

  mockTransferCallback = function(downloadSuccess) {
    info("mockTransferCallback");
    onTransferComplete(aWindow, downloadSuccess, destDir);
    destDir.remove(true);
    ok(!destDir.exists(), "Destination dir should be removed");
    ok(!destFile.exists(), "Destination file should be removed");
    mockTransferCallback = null;
    info("done mockTransferCallback");
  }

  function onUCTDialog(dialog) {
    function doLoad() {
      content.document.querySelector("iframe").remove();
    }
    testBrowser.messageManager.loadFrameScript("data:,(" + doLoad.toString() + ")()", false);
    executeSoon(continueDownloading);
  }

  function continueDownloading() {
    let windows = Services.wm.getEnumerator("");
    while (windows.hasMoreElements()) {
      let win = windows.getNext();
      if (win.location && win.location.href == UCT_URI) {
        win.document.documentElement._fireButtonEvent("accept");
        win.close();
        return;
      }
    }
    ok(false, "No Unknown Content Type dialog yet?");
  }

  function onTransferComplete(aWindow2, downloadSuccess) {
    ok(downloadSuccess, "Link should have been downloaded successfully");
    aWindow2.close();

    executeSoon(aCallback);
  }
}


var windowObserver = {
  setCallback(aCallback) {
    if (this._callback) {
      ok(false, "Should only be dealing with one callback at a time.");
    }
    this._callback = aCallback;
  },
  observe(aSubject, aTopic, aData) {
    if (aTopic != "domwindowopened") {
      return;
    }

    let win = aSubject.QueryInterface(Ci.nsIDOMEventTarget);

    win.addEventListener("load", function(event) {
      if (win.location == UCT_URI) {
        SimpleTest.executeSoon(function() {
          if (windowObserver._callback) {
            windowObserver._callback(win);
            delete windowObserver._callback;
          } else {
            ok(false, "Unexpected UCT dialog!");
          }
        });
      }
    }, {once: true});
  }
};

Services.ww.registerNotification(windowObserver);

function test() {
  waitForExplicitFinish();

  function testOnWindow(options, callback) {
    info("testOnWindow(" + options + ")");
    var win = OpenBrowserWindow(options);
    info("got " + win);
    whenDelayedStartupFinished(win, () => callback(win));
  }

  function whenDelayedStartupFinished(aWindow, aCallback) {
    info("whenDelayedStartupFinished");
    Services.obs.addObserver(function observer(aSubject, aTopic) {
      info("whenDelayedStartupFinished, got topic: " + aTopic + ", got subject: " + aSubject + ", waiting for " + aWindow);
      if (aWindow == aSubject) {
        Services.obs.removeObserver(observer, aTopic);
        executeSoon(aCallback);
        info("whenDelayedStartupFinished found our window");
      }
    }, "browser-delayed-startup-finished", false);
  }

  mockTransferRegisterer.register();

  registerCleanupFunction(function() {
    info("Running the cleanup code");
    mockTransferRegisterer.unregister();
    MockFilePicker.cleanup();
    Services.ww.unregisterNotification(windowObserver);
    Services.prefs.clearUserPref(ALWAYS_DOWNLOAD_DIR_PREF);
    Services.prefs.clearUserPref(SAVE_PER_SITE_PREF);
    info("Finished running the cleanup code");
  });

  Services.prefs.setBoolPref(ALWAYS_DOWNLOAD_DIR_PREF, false);
  testOnWindow(undefined, function(win) {
    let windowGonePromise = promiseWindowWillBeClosed(win);
    Services.prefs.setBoolPref(SAVE_PER_SITE_PREF, true);
    triggerSave(win, function() {
      windowGonePromise.then(function() {
        Services.prefs.setBoolPref(SAVE_PER_SITE_PREF, false);
        testOnWindow(undefined, function(win2) {
          triggerSave(win2, finish);
        });
      });
    });
  });
}
