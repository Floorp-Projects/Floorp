/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var MockFilePicker = SpecialPowers.MockFilePicker;
MockFilePicker.init(window);

let tempScope = {};
Cu.import("resource://gre/modules/NetUtil.jsm", tempScope);
let NetUtil = tempScope.NetUtil;

// Trigger a save of a link in public mode, then trigger an identical save
// in private mode and ensure that the second request is differentiated from
// the first by checking that cookies set by the first response are not sent
// during the second request.
function triggerSave(aWindow, aCallback) {
  var fileName;
  let testBrowser = aWindow.gBrowser.selectedBrowser;
  // This page sets a cookie if and only if a cookie does not exist yet
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

    mockTransferCallback = function(downloadSuccess) {
      onTransferComplete(aWindow, downloadSuccess, destDir);
      destDir.remove(true);
      ok(!destDir.exists(), "Destination dir should be removed");
      ok(!destFile.exists(), "Destination file should be removed");
      mockTransferCallback = function(){};
    }

    // Select "Save Link As" option from context menu
    var saveLinkCommand = aWindow.document.getElementById("context-savelink");
    saveLinkCommand.doCommand();

    event.target.hidePopup();
  }

  function onTransferComplete(aWindow, downloadSuccess, destDir) {
    ok(downloadSuccess, "Link should have been downloaded successfully");
    aWindow.gBrowser.removeCurrentTab();

    executeSoon(function() aCallback());
  }
}

function test() {
  waitForExplicitFinish();

  var windowsToClose = [];
  var gNumSet = 0;
  function testOnWindow(options, callback) {
    var win = OpenBrowserWindow(options);
    whenDelayedStartupFinished(win, () => callback(win));
  }

  function whenDelayedStartupFinished(aWindow, aCallback) {
    Services.obs.addObserver(function observer(aSubject, aTopic) {
      if (aWindow == aSubject) {
        Services.obs.removeObserver(observer, aTopic);
        executeSoon(aCallback);
      }
    }, "browser-delayed-startup-finished", false);
  }

  mockTransferRegisterer.register();

  registerCleanupFunction(function () {
    mockTransferRegisterer.unregister();
    MockFilePicker.cleanup();
    windowsToClose.forEach(function(win) {
      win.close();
    });
    Services.obs.removeObserver(observer, "http-on-modify-request");
    Services.obs.removeObserver(observer, "http-on-examine-response");
  });
 
  function observer(subject, topic, state) {
    if (topic == "http-on-modify-request") {
      onModifyRequest(subject);
    } else if (topic == "http-on-examine-response") {
      onExamineResponse(subject);
    }
  }

  function onExamineResponse(subject) {
    let channel = subject.QueryInterface(Ci.nsIHttpChannel);
    try {
      let cookies = channel.getResponseHeader("set-cookie");
      // From browser/base/content/test/bug792715.sjs, we receive a Set-Cookie
      // header with foopy=1 when there are no cookies for that domain.
      is(cookies, "foopy=1", "Cookie should be foopy=1");
      gNumSet += 1;
    } catch (ex if ex.result == Cr.NS_ERROR_NOT_AVAILABLE) { }
  }

  function onModifyRequest(subject) {
    let channel = subject.QueryInterface(Ci.nsIHttpChannel);
    try {
      let cookies = channel.getRequestHeader("cookie");
      // From browser/base/content/test/bug792715.sjs, we should never send a
      // cookie because we are making only 2 requests: one in public mode, and
      // one in private mode.
      throw "We should never send a cookie in this test";
    } catch (ex if ex.result == Cr.NS_ERROR_NOT_AVAILABLE) { }
  }

  Services.obs.addObserver(observer, "http-on-modify-request", false);
  Services.obs.addObserver(observer, "http-on-examine-response", false);

  testOnWindow(undefined, function(win) {
    // The first save from a regular window sets a cookie.
    triggerSave(win, function() {
      is(gNumSet, 1, "1 cookie should be set");

      // The second save from a private window also sets a cookie.
      testOnWindow({private: true}, function(win) {
        triggerSave(win, function() {
          is(gNumSet, 2, "2 cookies should be set");
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
