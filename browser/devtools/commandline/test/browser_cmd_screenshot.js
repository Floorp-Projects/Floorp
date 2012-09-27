/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that screenshot command works properly
const TEST_URI = "http://example.com/browser/browser/devtools/commandline/" +
                 "test/browser_cmd_screenshot.html";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;
let tempScope = {};
Cu.import("resource://gre/modules/FileUtils.jsm", tempScope);
let FileUtils = tempScope.FileUtils;

let pb = Cc["@mozilla.org/privatebrowsing;1"]
           .getService(Ci.nsIPrivateBrowsingService);
function test() {
  DeveloperToolbarTest.test(TEST_URI, [ testInput, testCapture ]);
}

function testInput() {
  helpers.setInput('screenshot');
  helpers.check({
    input:  'screenshot',
    markup: 'VVVVVVVVVV',
    status: 'VALID',
    args: {
    }
  });

  helpers.setInput('screenshot abc.png');
  helpers.check({
    input:  'screenshot abc.png',
    markup: 'VVVVVVVVVVVVVVVVVV',
    status: 'VALID',
    args: {
      filename: { value: "abc.png"},
    }
  });

  helpers.setInput('screenshot --fullpage');
  helpers.check({
    input:  'screenshot --fullpage',
    markup: 'VVVVVVVVVVVVVVVVVVVVV',
    status: 'VALID',
    args: {
      fullpage: { value: true},
    }
  });

  helpers.setInput('screenshot abc --delay 5');
  helpers.check({
    input:  'screenshot abc --delay 5',
    markup: 'VVVVVVVVVVVVVVVVVVVVVVVV',
    status: 'VALID',
    args: {
      filename: { value: "abc"},
      delay: { value: "5"},
    }
  });

  helpers.setInput('screenshot --selector img#testImage');
  helpers.check({
    input:  'screenshot --selector img#testImage',
    markup: 'VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV',
    status: 'VALID',
    args: {
      selector: { value: content.document.getElementById("testImage")},
    }
  });
}

function testCapture() {
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

  let PBEntered = DeveloperToolbarTest.checkCalled(function() {
    Services.obs.removeObserver(PBEntered,
                                "private-browsing-transition-complete",
                                false);

    Services.obs.addObserver(PBLeft, "last-pb-context-exited", false);

    DeveloperToolbarTest.exec({
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

    ok(checkClipboard(), "Screenshot present in clipboard in private browsing");

    pb.privateBrowsingEnabled = false;
  });

  let PBLeft = DeveloperToolbarTest.checkCalled(function() {
    Services.obs.removeObserver(PBLeft, "last-pb-context-exited", false);
    executeSoon(function() {
      ok(!checkClipboard(), "Screenshot taken in private browsing mode is not" +
                            " present outside of it in the clipboard");
      Services.prefs.clearUserPref("browser.privatebrowsing.keep_current_session");
      pb = null;
    });
  });

  let path = FileUtils.getFile("TmpD", ["TestScreenshotFile.png"]).path;

  DeveloperToolbarTest.exec({
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

  Services.obs.addObserver(PBEntered, "private-browsing-transition-complete",
                           false);

  executeSoon(function() {
    ok(checkTemporaryFile(), "Screenshot got created");

    DeveloperToolbarTest.exec({
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

    Services.prefs.setBoolPref("browser.privatebrowsing.keep_current_session", true);

    pb.privateBrowsingEnabled = true;
  });
}
