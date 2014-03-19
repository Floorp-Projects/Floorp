/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://services-sync/healthreport.jsm", this);
Cu.import("resource://testing-common/services/healthreport/utils.jsm", this);
Cu.import("resource://gre/modules/services/healthreport/providers.jsm");

const EXPERIMENT1_ID       = "test-experiment-1@tests.mozilla.org";
const EXPERIMENT1_XPI_SHA1 = "sha1:08c4d3ef1d0fc74faa455e85106ef0bc8cf8ca90";
const EXPERIMENT1_XPI_NAME = "experiment-1.xpi";
const EXPERIMENT1_NAME     = "Test experiment 1";

const EXPERIMENT1A_XPI_SHA1 = "sha1:2b8d14e3e06a54d5ce628fe3598cbb364cff9e6b";
const EXPERIMENT1A_XPI_NAME = "experiment-1a.xpi";
const EXPERIMENT1A_NAME     = "Test experiment 1.1";

const EXPERIMENT2_ID       = "test-experiment-2@tests.mozilla.org"
const EXPERIMENT2_XPI_SHA1 = "sha1:81877991ec70360fb48db84c34a9b2da7aa41d6a";
const EXPERIMENT2_XPI_NAME = "experiment-2.xpi";

let gAppInfo = null;

function getReporter(name, uri, inspected) {
  return Task.spawn(function init() {
    let reporter = getHealthReporter(name, uri, inspected);
    yield reporter.init();

    yield reporter._providerManager.registerProviderFromType(
      HealthReportProvider);

    throw new Task.Result(reporter);
  });
}

function removeCacheFile() {
  let path = OS.Path.join(OS.Constants.Path.profileDir, "experiments.json");
  return OS.File.remove(path);
}

function disableCertificateChecks() {
  let pref = "experiments.manifest.cert.checkAttributes";
  Services.prefs.setBoolPref(pref, false);
  do_register_cleanup(() => Services.prefs.clearUserPref(pref));
}

function patchPolicy(policy, data) {
  for (let key of Object.keys(data)) {
    Object.defineProperty(policy, key, {
      value: data[key],
      writable: true,
    });
  }
}

function defineNow(policy, time) {
  patchPolicy(policy, { now: () => new Date(time) });
}

function futureDate(date, offset) {
  return new Date(date.getTime() + offset);
}

function dateToSeconds(date) {
  return date.getTime() / 1000;
}

function createAppInfo(options) {
  const XULAPPINFO_CONTRACTID = "@mozilla.org/xre/app-info;1";
  const XULAPPINFO_CID = Components.ID("{c763b610-9d49-455a-bbd2-ede71682a1ac}");

  let options = options || {};
  let id = options.id || "xpcshell@tests.mozilla.org";
  let name = options.name || "XPCShell";
  let version = options.version || "1.0";
  let platformVersion = options.platformVersion || "1.0";
  let date = options.date || new Date();

  let buildID = "" + date.getYear() + date.getMonth() + date.getDate() + "01";

  gAppInfo = {
    // nsIXULAppInfo
    vendor: "Mozilla",
    name: name,
    ID: id,
    version: version,
    appBuildID: buildID,
    platformVersion: platformVersion ? platformVersion : "1.0",
    platformBuildID: buildID,

    // nsIXULRuntime
    inSafeMode: false,
    logConsoleErrors: true,
    OS: "XPCShell",
    XPCOMABI: "noarch-spidermonkey",
    invalidateCachesOnRestart: function invalidateCachesOnRestart() {
      // Do nothing
    },

    // nsICrashReporter
    annotations: {},

    annotateCrashReport: function(key, data) {
      this.annotations[key] = data;
    },

    QueryInterface: XPCOMUtils.generateQI([Ci.nsIXULAppInfo,
                                           Ci.nsIXULRuntime,
                                           Ci.nsICrashReporter,
                                           Ci.nsISupports])
  };

  let XULAppInfoFactory = {
    createInstance: function (outer, iid) {
      if (outer != null) {
        throw Cr.NS_ERROR_NO_AGGREGATION;
      }
      return gAppInfo.QueryInterface(iid);
    }
  };

  let registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
  registrar.registerFactory(XULAPPINFO_CID, "XULAppInfo",
                            XULAPPINFO_CONTRACTID, XULAppInfoFactory);
}
