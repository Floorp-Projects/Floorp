/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var MockFilePicker = SpecialPowers.MockFilePicker;
MockFilePicker.init(window);

let tempScope = {};
Cu.import("resource://gre/modules/NetUtil.jsm", tempScope);
let NetUtil = tempScope.NetUtil;

// Trigger a save of a link in public mode, then trigger an identical save
// in private mode and ensure that the second request is differentiated from
// the first by checking the cookies that are sent.

function triggerSave(aWindow, aCallback) {
  var fileName;
  let testBrowser = aWindow.gBrowser.selectedBrowser;
  testBrowser.loadURI("http://mochi.test:8888/browser/browser/base/content/test/bug792517-2.html");
  testBrowser.addEventListener("pageshow", function pageShown(event) {
    if (event.target.location == "about:blank")
      return;
    testBrowser.removeEventListener("pageshow", pageShown, false);

    executeSoon(function () {
      aWindow.document.addEventListener("popupshown", function(e) contextMenuOpened(aWindow, e), false);

      var link = testBrowser.contentDocument.getElementById("fff");
      EventUtils.synthesizeMouseAtCenter(link,
                                         { type: "contextmenu", button: 2 },
                                         testBrowser.contentWindow);
    });
  }, false);

  function contextMenuOpened(aWindow, event) {
    event.currentTarget.removeEventListener("popupshown", contextMenuOpened, false);

    // Create the folder the link will be saved into.
    var destDir = createTemporarySaveDirectory();
    var destFile = destDir.clone();

    MockFilePicker.displayDirectory = destDir;
    MockFilePicker.showCallback = function(fp) {
      fileName = fp.defaultString;
      destFile.append (fileName);
      MockFilePicker.returnFiles = [destFile];
      MockFilePicker.filterIndex = 1; // kSaveAsType_URL
    };

    mockTransferCallback = function(a) {
      onTransferComplete(aWindow, a, destFile, destDir);
      mockTransferCallback = function(){};
    }

    // Select "Save Link As" option from context menu
    var saveLinkCommand = aWindow.document.getElementById("context-savelink");
    saveLinkCommand.doCommand();

    event.target.hidePopup();
  }

  function onTransferComplete(aWindow, downloadSuccess, destFile, destDir) {
    ok(downloadSuccess, "Link should have been downloaded successfully");
    aWindow.gBrowser.removeCurrentTab();

    // Give the request a chance to finish
    executeSoon(function() aCallback(destFile, destDir));
  }
}

function readFile(file, callback) {
  let channel = NetUtil.newChannel(file);
  channel.contentType = "application/javascript";

  NetUtil.asyncFetch(channel, function(inputStream, status) {
    ok(Components.isSuccessCode(status),
       "file was read successfully");

    let content = NetUtil.readInputStreamToString(inputStream,
                                                  inputStream.available());
    executeSoon(function() callback(content));
  });
}

function test() {
  waitForExplicitFinish();

  var windowsToClose = [];
  function testOnWindow(options, callback) {
    var win = OpenBrowserWindow(options);
    win.addEventListener("load", function onLoad() {
      win.removeEventListener("load", onLoad, false);
      windowsToClose.push(win);
      executeSoon(function() callback(win));
    }, false);
  }

  mockTransferRegisterer.register();

  registerCleanupFunction(function () {
    mockTransferRegisterer.unregister();
    MockFilePicker.cleanup();
    windowsToClose.forEach(function(win) {
      win.close();
    });
  });

  testOnWindow(undefined, function(win) {
    triggerSave(win, function(destFile, destDir) {
      readFile(destFile, function(content) {
        is(content, "cookie-not-present", "no cookie should be sent");
        destDir.remove(true);

        testOnWindow({private: true}, function(win) {
          triggerSave(win, function(destFile, destDir) {
            readFile(destFile, function(content) {
              is(content, "cookie-not-present", "no cookie should be sent");
              destDir.remove(true);
              finish();
            });
          });
        });
      });
    });
  });
}

Cc["@mozilla.org/moz/jssubscript-loader;1"]
  .getService(Ci.mozIJSSubScriptLoader)
  .loadSubScript("chrome://mochitests/content/browser/toolkit/content/tests/browser/common/mockTransfer.js",
                 this);

function createTemporarySaveDirectory() {
  var saveDir = Cc["@mozilla.org/file/directory_service;1"]
                  .getService(Ci.nsIProperties)
                  .get("TmpD", Ci.nsIFile);
  saveDir.append("testsavedir");
  if (!saveDir.exists())
    saveDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0755);
  return saveDir;
}
