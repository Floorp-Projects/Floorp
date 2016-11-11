/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

XPCOMUtils.defineLazyModuleGetter(this, "Promise",
  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
  "resource://gre/modules/Task.jsm");

const CHROME_BASE = "chrome://mochitests/content/browser/browser/base/content/test/general/";
const HTTPS_BASE = "https://example.com/browser/browser/base/content/test/general/";

const TELEMETRY_LOG_PREF = "toolkit.telemetry.log.level";
const telemetryOriginalLogPref = Preferences.get(TELEMETRY_LOG_PREF, null);

const originalReportUrl = Services.prefs.getCharPref("datareporting.healthreport.about.reportUrl");

registerCleanupFunction(function() {
  // Ensure we don't pollute prefs for next tests.
  if (telemetryOriginalLogPref) {
    Preferences.set(TELEMETRY_LOG_PREF, telemetryOriginalLogPref);
  } else {
    Preferences.reset(TELEMETRY_LOG_PREF);
  }

  try {
    Services.prefs.setCharPref("datareporting.healthreport.about.reportUrl", originalReportUrl);
    Services.prefs.setBoolPref("datareporting.healthreport.uploadEnabled", true);
  } catch (ex) {}
});

function fakeTelemetryNow(...args) {
  let date = new Date(...args);
  let scope = {};
  const modules = [
    Cu.import("resource://gre/modules/TelemetrySession.jsm", scope),
    Cu.import("resource://gre/modules/TelemetryEnvironment.jsm", scope),
    Cu.import("resource://gre/modules/TelemetryController.jsm", scope),
  ];

  for (let m of modules) {
    m.Policy.now = () => new Date(date);
  }

  return date;
}

function* setupPingArchive() {
  let scope = {};
  Cu.import("resource://gre/modules/TelemetryController.jsm", scope);
  Cc["@mozilla.org/moz/jssubscript-loader;1"].getService(Ci.mozIJSSubScriptLoader)
    .loadSubScript(CHROME_BASE + "healthreport_pingData.js", scope);

  for (let p of scope.TEST_PINGS) {
    fakeTelemetryNow(p.date);
    p.id = yield scope.TelemetryController.submitExternalPing(p.type, p.payload);
  }
}

var gTests = [

{
  desc: "Test the remote commands",
  setup: Task.async(function*()
  {
    Preferences.set(TELEMETRY_LOG_PREF, "Trace");
    yield setupPingArchive();
    Preferences.set("datareporting.healthreport.about.reportUrl",
                    HTTPS_BASE + "healthreport_testRemoteCommands.html");
  }),
  run: function(iframe)
  {
    let deferred = Promise.defer();
    let results = 0;
    try {
      iframe.contentWindow.addEventListener("FirefoxHealthReportTestResponse", function evtHandler(event) {
        let data = event.detail.data;
        if (data.type == "testResult") {
          ok(data.pass, data.info);
          results++;
        }
        else if (data.type == "testsComplete") {
          is(results, data.count, "Checking number of results received matches the number of tests that should have run");
          iframe.contentWindow.removeEventListener("FirefoxHealthReportTestResponse", evtHandler, true);
          deferred.resolve();
        }
      }, true);

    } catch (e) {
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

  // xxxmpc leaving this here until we resolve bug 854038 and bug 854060
  requestLongerTimeout(10);

  Task.spawn(function* () {
    for (let testCase of gTests) {
      info(testCase.desc);
      yield testCase.setup();

      let iframe = yield promiseNewTabLoadEvent("about:healthreport");

      yield testCase.run(iframe);

      gBrowser.removeCurrentTab();
    }

    finish();
  });
}

function promiseNewTabLoadEvent(aUrl, aEventType = "load")
{
  let deferred = Promise.defer();
  let tab = gBrowser.selectedTab = gBrowser.addTab(aUrl);
  tab.linkedBrowser.addEventListener(aEventType, function load(event) {
    tab.linkedBrowser.removeEventListener(aEventType, load, true);
    let iframe = tab.linkedBrowser.contentDocument.getElementById("remote-report");
      iframe.addEventListener("load", function frameLoad(e) {
        if (iframe.contentWindow.location.href == "about:blank" ||
            e.target != iframe) {
          return;
        }
        iframe.removeEventListener("load", frameLoad, false);
        deferred.resolve(iframe);
      }, false);
    }, true);
  return deferred.promise;
}
