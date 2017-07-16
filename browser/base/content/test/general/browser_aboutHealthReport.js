/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */


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

async function setupPingArchive() {
  let scope = {};
  Cu.import("resource://gre/modules/TelemetryController.jsm", scope);
  Cc["@mozilla.org/moz/jssubscript-loader;1"].getService(Ci.mozIJSSubScriptLoader)
    .loadSubScript(CHROME_BASE + "healthreport_pingData.js", scope);

  for (let p of scope.TEST_PINGS) {
    fakeTelemetryNow(p.date);
    p.id = await scope.TelemetryController.submitExternalPing(p.type, p.payload);
  }
}

var gTests = [

{
  desc: "Test the remote commands",
  async setup() {
    Preferences.set(TELEMETRY_LOG_PREF, "Trace");
    await setupPingArchive();
    Preferences.set("datareporting.healthreport.about.reportUrl",
                    HTTPS_BASE + "healthreport_testRemoteCommands.html");
  },
  run(iframe) {
    return new Promise((resolve, reject) => {
      let results = 0;
      try {
        iframe.contentWindow.addEventListener("FirefoxHealthReportTestResponse", function evtHandler(event) {
          let data = event.detail.data;
          if (data.type == "testResult") {
            ok(data.pass, data.info);
            results++;
          } else if (data.type == "testsComplete") {
            is(results, data.count, "Checking number of results received matches the number of tests that should have run");
            iframe.contentWindow.removeEventListener("FirefoxHealthReportTestResponse", evtHandler, true);
            resolve();
          }
        }, true);

      } catch (e) {
        ok(false, "Failed to get all commands");
        reject();
      }
    });
  }
},

]; // gTests

function test() {
  waitForExplicitFinish();

  // xxxmpc leaving this here until we resolve bug 854038 and bug 854060
  requestLongerTimeout(10);

  (async function() {
    for (let testCase of gTests) {
      info(testCase.desc);
      await testCase.setup();

      let iframe = await promiseNewTabLoadEvent("about:healthreport");

      await testCase.run(iframe);

      gBrowser.removeCurrentTab();
    }

    finish();
  })();
}

function promiseNewTabLoadEvent(aUrl, aEventType = "load") {
  return new Promise(resolve => {
    let tab = gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, aUrl);
    tab.linkedBrowser.addEventListener(aEventType, function(event) {
      let iframe = tab.linkedBrowser.contentDocument.getElementById("remote-report");
        iframe.addEventListener("load", function frameLoad(e) {
          if (iframe.contentWindow.location.href == "about:blank" ||
              e.target != iframe) {
            return;
          }
          iframe.removeEventListener("load", frameLoad);
          resolve(iframe);
        });
      }, {capture: true, once: true});
  });
}
