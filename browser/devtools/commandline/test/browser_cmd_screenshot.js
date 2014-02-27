/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that screenshot command works properly
const TEST_URI = "http://example.com/browser/browser/devtools/commandline/" +
                 "test/browser_cmd_screenshot.html";

let FileUtils = (Cu.import("resource://gre/modules/FileUtils.jsm", {})).FileUtils;

function test() {
  return Task.spawn(spawnTest).then(finish, helpers.handleError);
}

function spawnTest() {
  waitForExplicitFinish();

  info("RUN TEST: non-private window");
  let normWin = yield addWindow({ private: false });
  yield addTabWithToolbarRunTests(normWin);
  normWin.close();

  info("RUN TEST: private window");
  let pbWin = yield addWindow({ private: true });
  yield addTabWithToolbarRunTests(pbWin);
  pbWin.close();
}

function addTabWithToolbarRunTests(win) {
  let options = yield helpers.openTab(TEST_URI, { chromeWindow: win });
  yield helpers.openToolbar(options);

  // Test input status
  yield helpers.audit(options, [
    {
      setup: 'screenshot',
      check: {
        input:  'screenshot',
        markup: 'VVVVVVVVVV',
        status: 'VALID',
        args: {
        }
      },
    },
    {
      setup: 'screenshot abc.png',
      check: {
        input:  'screenshot abc.png',
        markup: 'VVVVVVVVVVVVVVVVVV',
        status: 'VALID',
        args: {
          filename: { value: "abc.png"},
        }
      },
    },
    {
      setup: 'screenshot --fullpage',
      check: {
        input:  'screenshot --fullpage',
        markup: 'VVVVVVVVVVVVVVVVVVVVV',
        status: 'VALID',
        args: {
          fullpage: { value: true},
        }
      },
    },
    {
      setup: 'screenshot abc --delay 5',
      check: {
        input:  'screenshot abc --delay 5',
        markup: 'VVVVVVVVVVVVVVVVVVVVVVVV',
        status: 'VALID',
        args: {
          filename: { value: "abc"},
          delay: { value: 5 },
        }
      },
    },
    {
      setup: 'screenshot --selector img#testImage',
      check: {
        input:  'screenshot --selector img#testImage',
        markup: 'VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV',
        status: 'VALID',
        args: {
          selector: {
            value: options.window.document.getElementById("testImage")
          },
        }
      },
    },
  ]);

  // Test capture to file
  let file = FileUtils.getFile("TmpD", [ "TestScreenshotFile.png" ]);

  yield helpers.audit(options, [
    {
      setup: 'screenshot ' + file.path,
      check: {
        args: {
          filename: { value: "" + file.path },
          fullpage: { value: false },
          clipboard: { value: false },
          chrome: { value: false },
        },
      },
      exec: {
        output: new RegExp("^Saved to "),
      },
      post: function() {
        // Bug 849168: screenshot command tests fail in try but not locally
        // ok(file.exists(), "Screenshot file exists");

        if (file.exists()) {
          file.remove(false);
        }
      }
    },
  ]);

  // Test capture to clipboard
  let clipid = Ci.nsIClipboard;
  let clip = Cc["@mozilla.org/widget/clipboard;1"].getService(clipid);
  let trans = Cc["@mozilla.org/widget/transferable;1"]
                .createInstance(Ci.nsITransferable);
  trans.init(null);
  trans.addDataFlavor("image/png");

  yield helpers.audit(options, [
    {
      setup: 'screenshot --fullpage --clipboard',
      check: {
        args: {
          fullpage: { value: true },
          clipboard: { value: true },
          chrome: { value: false },
        },
      },
      exec: {
        output: new RegExp("^Copied to clipboard.$"),
      },
      post: function() {
        try {
          clip.getData(trans, clipid.kGlobalClipboard);
          let str = new Object();
          let strLength = new Object();
          trans.getTransferData("image/png", str, strLength);

          ok(str.value, "screenshot exists");
          ok(strLength.value > 0, "screenshot has length");
        }
        finally {
          Services.prefs.setBoolPref("browser.privatebrowsing.keep_current_session", true);

          // Recent PB changes to the test I'm modifying removed the 'pb'
          // variable, but left this line in tact. This seems so obviously
          // wrong that I'm leaving this in in case the analysis is wrong
          // pb.privateBrowsingEnabled = true;
        }
      }
    },
  ]);

  yield helpers.closeToolbar(options);
  yield helpers.closeTab(options);
}

function addWindow(windowOptions) {
  let deferred = promise.defer();

  let win = OpenBrowserWindow(windowOptions);

  // This feels hacky, we should refactor it
  whenDelayedStartupFinished(win, function() {
    // Would like to get rid of this executeSoon, but without it the url
    // (TEST_URI) provided in addTabWithToolbarRunTests hasn't loaded
    executeSoon(function() {
      deferred.resolve(win);
    });
  });

  return deferred.promise;
}
