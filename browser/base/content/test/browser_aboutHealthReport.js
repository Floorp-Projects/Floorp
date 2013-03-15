/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

XPCOMUtils.defineLazyModuleGetter(this, "Promise",
  "resource://gre/modules/commonjs/sdk/core/promise.js");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
  "resource://gre/modules/Task.jsm");

registerCleanupFunction(function() {
  // Ensure we don't pollute prefs for next tests.
  try {
    Services.prefs.clearUserPref("datareporting.healthreport.about.reportUrl");
    let policy = Cc["@mozilla.org/datareporting/service;1"]
                 .getService(Ci.nsISupports)
                 .wrappedJSObject
                 .policy;
        policy.recordHealthReportUploadEnabled(true,
                                           "Resetting after tests.");
  } catch (ex) {}
});

let gTests = [

{
  desc: "Check that about:healthreport loads correctly",
  setup: function ()
  {
  },
  run: function ()
  {
    is(gBrowser.selectedTab.linkedBrowser.currentURI.spec, "about:healthreport", "Page loaded correctly");
  }
},


{
  desc: "Test the postMessage API",
  setup: function ()
  {
    Services.prefs.setCharPref("datareporting.healthreport.about.reportUrl", 
                               "https://example.com/browser/browser/base/content/test/healthreport_testPostMessage.html");
  },
  run: function ()
  {
    let deferred = Promise.defer();

    let acks = {}
    try {
      let win = gBrowser.contentWindow;
      win.addEventListener("message", function process(e) {
        acks[e.data.type] = true;
        if (Object.keys(acks).length == 2) {
          ok(acks["prefs"],   "Prefs sent ok");
          ok(acks["payload"], "Payload sent ok");
          win.removeEventListener("message", process, false, true);
          deferred.resolve();
        }
      }, false, true);

    } catch(e) {
      deferred.resolve()
    }
    return deferred.promise;
  }
},

{
  desc: "Test the remote commands",
  setup: function ()
  {
    Services.prefs.setCharPref("datareporting.healthreport.about.reportUrl",
                               "https://example.com/browser/browser/base/content/test/healthreport_testRemoteCommands.html");
  },
  run: function ()
  {
    let deferred = Promise.defer();

    let policy = Cc["@mozilla.org/datareporting/service;1"]
                 .getService(Ci.nsISupports)
                 .wrappedJSObject
                 .policy;

    let acks = {}
    try {
      let win = gBrowser.contentWindow;
      win.addEventListener("message", function testLoad(e) {
        if (!e.data.command)
          return;

        acks[e.data.command] = true;
        switch (e.data.command) {
          case "DisableDataSubmission":
            is(policy.healthReportUploadEnabled, false, "FHR successfully disabled");
            break;
          case "EnableDataSubmission":
            is(policy.healthReportUploadEnabled, true,  "FHR successfully enabled");
            break;
          case "RequestCurrentPrefs":
          case "RequestCurrentPayload":
            ok(true, e.data.command + " successfully processed");
        }
        if (Object.keys(acks).length == 4) {
          deferred.resolve();
        }
      }, false, true);

    } catch(e) {
      ok(false, "Failed to get all commands");
      deferred.reject();
    }
    return deferred.promise;
  }
},


]; // gTests

function test()
{
  waitForExplicitFinish();

  Task.spawn(function () {
    for (let test of gTests) {
      info(test.desc);
      test.setup();

      yield promiseNewTabLoadEvent("about:healthreport");

      yield test.run();

      gBrowser.removeCurrentTab();
    }

    finish();
  });
}

function promiseNewTabLoadEvent(aUrl, aEventType="load")
{
  let deferred = Promise.defer();
  let tab = gBrowser.selectedTab = gBrowser.addTab(aUrl);
  tab.linkedBrowser.addEventListener(aEventType, function load(event) {
    tab.linkedBrowser.removeEventListener(aEventType, load, true);
    let iframe = tab.linkedBrowser.contentDocument.getElementById("remote-report");
      iframe.addEventListener("load", function frameLoad(e) {
        iframe.removeEventListener("load", frameLoad, false);
        deferred.resolve();
      }, false);
    }, true);
  return deferred.promise;
}

