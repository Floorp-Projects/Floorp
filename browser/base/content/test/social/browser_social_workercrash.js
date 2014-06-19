/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This tests our recovery if a child content process hosting providers
// crashes.

// A content script we inject into one of our browsers
const TEST_CONTENT_HELPER = "chrome://mochitests/content/browser/browser/base/content/test/social/social_crash_content_helper.js";

let {getFrameWorkerHandle} = Cu.import("resource://gre/modules/FrameWorker.jsm", {});
let {Promise} = Cu.import("resource://gre/modules/Promise.jsm", {}).Promise;

function test() {
  waitForExplicitFinish();

  // We need to ensure all our workers are in the same content process.
  Services.prefs.setIntPref("dom.ipc.processCount", 1);

  // This test generates many uncaught promises that should not cause failures.
  Promise.Debugging.clearUncaughtErrorObservers();

  runSocialTestWithProvider(gProviders, function (finishcb) {
    runSocialTests(tests, undefined, undefined, function() {
      Services.prefs.clearUserPref("dom.ipc.processCount");
      finishcb();
    });
  });
}

let gProviders = [
  {
    name: "provider 1",
    origin: "https://example.com",
    sidebarURL: "https://example.com/browser/browser/base/content/test/social/social_sidebar.html?provider1",
    workerURL: "https://example.com/browser/browser/base/content/test/social/social_worker.js",
    iconURL: "chrome://branding/content/icon48.png"
  },
  {
    name: "provider 2",
    origin: "https://test1.example.com",
    sidebarURL: "https://test1.example.com/browser/browser/base/content/test/social/social_sidebar.html?provider2",
    workerURL: "https://test1.example.com/browser/browser/base/content/test/social/social_worker.js",
    iconURL: "chrome://branding/content/icon48.png"
  }
];

var tests = {
  testCrash: function(next) {
    // open the sidebar, then crash the child.
    let sbrowser = document.getElementById("social-sidebar-browser");
    onSidebarLoad(function() {
      // get the browser element for our provider.
      let fw = getFrameWorkerHandle(gProviders[0].workerURL);
      fw.port.close();
      fw._worker.browserPromise.then(browser => {
        let mm = browser.messageManager;
        mm.loadFrameScript(TEST_CONTENT_HELPER, false);
        // add an observer for the crash - after it sees the crash we attempt
        // a reload.
        let observer = new crashObserver(function() {
          info("Saw the process crash.")
          Services.obs.removeObserver(observer, 'ipc:content-shutdown');
          // Add another sidebar load listener - it should be the error page.
          onSidebarLoad(function() {
            ok(sbrowser.contentDocument.location.href.indexOf("about:socialerror?")==0, "is on social error page");
            // after reloading, the sidebar should reload
            onSidebarLoad(function() {
              // now ping both workers - they should both be alive.
              ensureWorkerLoaded(gProviders[0], function() {
                ensureWorkerLoaded(gProviders[1], function() {
                  // and we are done!
                  next();
                });
              });
            });
            // click the try-again button.
            sbrowser.contentDocument.getElementById("btnTryAgain").click();
          });
        });
        Services.obs.addObserver(observer, 'ipc:content-shutdown', false);
        // and cause the crash.
        mm.sendAsyncMessage("social-test:crash");
      });
    })
    SocialSidebar.show();
  },
}

function onSidebarLoad(callback) {
  let sbrowser = document.getElementById("social-sidebar-browser");
  sbrowser.addEventListener("load", function load() {
    sbrowser.removeEventListener("load", load, true);
    callback();
  }, true);
}

function ensureWorkerLoaded(manifest, callback) {
  let fw = getFrameWorkerHandle(manifest.workerURL);
  // once the worker responds to a ping we know it must be up.
  let port = fw.port;
  port.onmessage = function(msg) {
    if (msg.data.topic == "pong") {
      port.close();
      callback();
    }
  }
  port.postMessage({topic: "ping"})
}

// More duplicated code from browser_thumbnails_brackground_crash.
// Bug 915518 exists to unify these.

// This observer is needed so we can clean up all evidence of the crash so
// the testrunner thinks things are peachy.
let crashObserver = function(callback) {
  this.callback = callback;
}
crashObserver.prototype = {
  observe: function(subject, topic, data) {
    is(topic, 'ipc:content-shutdown', 'Received correct observer topic.');
    ok(subject instanceof Components.interfaces.nsIPropertyBag2,
       'Subject implements nsIPropertyBag2.');
    // we might see this called as the process terminates due to previous tests.
    // We are only looking for "abnormal" exits...
    if (!subject.hasKey("abnormal")) {
      info("This is a normal termination and isn't the one we are looking for...");
      return;
    }

    var dumpID;
    if ('nsICrashReporter' in Components.interfaces) {
      dumpID = subject.getPropertyAsAString('dumpID');
      ok(dumpID, "dumpID is present and not an empty string");
    }

    if (dumpID) {
      var minidumpDirectory = getMinidumpDirectory();
      removeFile(minidumpDirectory, dumpID + '.dmp');
      removeFile(minidumpDirectory, dumpID + '.extra');
    }
    this.callback();
  }
}

function getMinidumpDirectory() {
  var dir = Services.dirsvc.get('ProfD', Components.interfaces.nsIFile);
  dir.append("minidumps");
  return dir;
}
function removeFile(directory, filename) {
  var file = directory.clone();
  file.append(filename);
  if (file.exists()) {
    file.remove(false);
  }
}
