/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var MockFilePicker = SpecialPowers.MockFilePicker;
MockFilePicker.init();

let tempScope = {};
Cu.import("resource://gre/modules/NetUtil.jsm", tempScope);
let NetUtil = tempScope.NetUtil;

// Trigger a save of a link in public mode, then trigger an identical save
// in private mode and ensure that the second request is differentiated from
// the first by checking the cookies that are sent.

function triggerSave(aCallback) {
  var fileName;
  gBrowser.loadURI("http://mochi.test:8888/browser/browser/base/content/test/bug792517-2.html");
  gBrowser.addEventListener("pageshow", function pageShown(event) {
    if (event.target.location == "about:blank")
      return;
    gBrowser.removeEventListener("pageshow", pageShown);

    executeSoon(function () {
      document.addEventListener("popupshown", contextMenuOpened);

      var link = gBrowser.contentDocument.getElementById("fff");
      EventUtils.synthesizeMouseAtCenter(link,
                                         { type: "contextmenu", button: 2 },
                                         gBrowser.contentWindow);
    });
  });

  function contextMenuOpened(event) {
    event.currentTarget.removeEventListener("popupshown", contextMenuOpened);

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

    mockTransferCallback = function(a) onTransferComplete(a, destFile, destDir);

    // Select "Save Link As" option from context menu
    var saveLinkCommand = document.getElementById("context-savelink");
    saveLinkCommand.doCommand();

    event.target.hidePopup();
  }

  function onTransferComplete(downloadSuccess, destFile, destDir) {
    ok(downloadSuccess, "Link should have been downloaded successfully");

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

  let pb = Cc["@mozilla.org/privatebrowsing;1"]
             .getService(Ci.nsIPrivateBrowsingService);

  mockTransferRegisterer.register();

  registerCleanupFunction(function () {
    mockTransferRegisterer.unregister();
    MockFilePicker.cleanup();
    pb.privateBrowsingEnabled = false;
    Services.prefs.clearUserPref("browser.privatebrowsing.keep_current_session");
    gBrowser.addTab();
    gBrowser.removeCurrentTab();
  });

  triggerSave(function(destFile, destDir) {
    readFile(destFile, function(content) {
      is(content, "cookie-not-present", "no cookie should be sent");
      destDir.remove(true);

      Services.prefs.setBoolPref("browser.privatebrowsing.keep_current_session", true);
      pb.privateBrowsingEnabled = true;
      triggerSave(function(destFile, destDir) {
        readFile(destFile, function(content) {
          is(content, "cookie-not-present", "no cookie should be sent");
          destDir.remove(true);
          finish();
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
