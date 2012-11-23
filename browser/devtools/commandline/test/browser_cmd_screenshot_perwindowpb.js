/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that screenshot command works properly
const TEST_URI = "http://example.com/browser/browser/devtools/commandline/" +
                 "test/browser_cmd_screenshot.html";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;
let FileUtils = Cu.import("resource://gre/modules/FileUtils.jsm", {}).FileUtils;

function test() {
  waitForExplicitFinish();

  function testOnWindow(aPrivate, aCallback) {
    let win = OpenBrowserWindow({private: aPrivate});
    win.addEventListener("load", function onLoad() {
      win.removeEventListener("load", onLoad, false);
      executeSoon(function() aCallback(win));
    }, false);
  };

  testOnWindow(false, function(win) {
    DeveloperToolbarTestPW.test(win, TEST_URI, [ testInput, testCapture ], null, function() {
      win.close();
      testOnWindow(true, function(win) {
        executeSoon(function() {
          DeveloperToolbarTestPW.test(win, TEST_URI, [ testInput, testCapture ], null, function() {
            win.close();
            finish();
          });
        })
      });
    });
  });

}

function testInput(aWindow) {
  helpers_perwindowpb.setInput('screenshot');
  helpers_perwindowpb.check({
    input:  'screenshot',
    markup: 'VVVVVVVVVV',
    status: 'VALID',
    args: {
    }
  });

  helpers_perwindowpb.setInput('screenshot abc.png');
  helpers_perwindowpb.check({
    input:  'screenshot abc.png',
    markup: 'VVVVVVVVVVVVVVVVVV',
    status: 'VALID',
    args: {
      filename: { value: "abc.png"},
    }
  });

  helpers_perwindowpb.setInput('screenshot --fullpage');
  helpers_perwindowpb.check({
    input:  'screenshot --fullpage',
    markup: 'VVVVVVVVVVVVVVVVVVVVV',
    status: 'VALID',
    args: {
      fullpage: { value: true},
    }
  });

  helpers_perwindowpb.setInput('screenshot abc --delay 5');
  helpers_perwindowpb.check({
    input:  'screenshot abc --delay 5',
    markup: 'VVVVVVVVVVVVVVVVVVVVVVVV',
    status: 'VALID',
    args: {
      filename: { value: "abc"},
      delay: { value: "5"},
    }
  });

  helpers_perwindowpb.setInput('screenshot --selector img#testImage');
  helpers_perwindowpb.check({
    input:  'screenshot --selector img#testImage',
    markup: 'VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV',
    status: 'VALID',
    args: {
      selector: { value: aWindow.content.document.getElementById("testImage")},
    }
  });
}

function testCapture(aWindow) {
  function checkTemporaryFile() {
    // Create a temporary file.
    let gFile = FileUtils.getFile("TmpD", ["TestScreenshotFile.png"]);
    if (gFile.exists()) {
      gFile.remove(false);
      return true;
    }
    else {
      return false;
    }
  }

  function checkClipboard() {
    try {
      let clipid = Ci.nsIClipboard;
      let clip = Cc["@mozilla.org/widget/clipboard;1"].getService(clipid);
      let trans = Cc["@mozilla.org/widget/transferable;1"]
                    .createInstance(Ci.nsITransferable);
      trans.init(null);
      trans.addDataFlavor("image/png");
      clip.getData(trans, clipid.kGlobalClipboard);
      let str = new Object();
      let strLength = new Object();
      trans.getTransferData("image/png", str, strLength);
      if (str.value && strLength.value > 0) {
        return true;
      }
    }
    catch (ex) {}
    return false;
  }

  let path = FileUtils.getFile("TmpD", ["TestScreenshotFile.png"]).path;

  DeveloperToolbarTestPW.exec(aWindow, {
    typed: "screenshot " + path,
    args: {
      delay: 0,
      filename: "" + path,
      fullpage: false,
      clipboard: false,
      node: null,
      chrome: false,
    },
    outputMatch: new RegExp("^Saved to "),
  });

  executeSoon(function() {
    ok(checkTemporaryFile(), "Screenshot got created");
  });

  DeveloperToolbarTestPW.exec(aWindow, {
    typed: "screenshot --fullpage --clipboard",
    args: {
      delay: 0,
      filename: " ",
      fullpage: true,
      clipboard: true,
      node: null,
      chrome: false,
    },
    outputMatch: new RegExp("^Copied to clipboard.$"),
  });

  ok(checkClipboard(), "Screenshot got created and copied");

  DeveloperToolbarTestPW.exec(aWindow, {
    typed: "screenshot --clipboard",
    args: {
      delay: 0,
      filename: " ",
      fullpage: false,
      clipboard: true,
      node: null,
      chrome: false,
    },
    outputMatch: new RegExp("^Copied to clipboard.$"),
  });

  ok(checkClipboard(), "Screenshot present in clipboard");
}
