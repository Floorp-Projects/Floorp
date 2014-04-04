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

function getExperimentPath(base) {
  let p = do_get_cwd();
  p.append(base);
  return p.path;
}

function sha1File(path) {
  let f = Cc["@mozilla.org/file/local;1"]
            .createInstance(Ci.nsILocalFile);
  f.initWithPath(path);
  let hasher = Cc["@mozilla.org/security/hash;1"]
                 .createInstance(Ci.nsICryptoHash);
  hasher.init(hasher.SHA1);

  let is = Cc["@mozilla.org/network/file-input-stream;1"]
             .createInstance(Ci.nsIFileInputStream);
  is.init(f, -1, 0, 0);
  hasher.updateFromStream(is, Math.pow(2, 32) - 1);
  is.close();
  let bytes = hasher.finish(false);

  return [("0" + bytes.charCodeAt(byte).toString(16)).slice(-2)
          for (byte in bytes)]
         .join("");
}

const EXPERIMENT1_ID       = "test-experiment-1@tests.mozilla.org";
const EXPERIMENT1_XPI_NAME = "experiment-1.xpi";
const EXPERIMENT1_NAME     = "Test experiment 1";
const EXPERIMENT1_PATH     = getExperimentPath(EXPERIMENT1_XPI_NAME);
const EXPERIMENT1_XPI_SHA1 = "sha1:" + sha1File(EXPERIMENT1_PATH);


const EXPERIMENT1A_XPI_NAME = "experiment-1a.xpi";
const EXPERIMENT1A_NAME     = "Test experiment 1.1";
const EXPERIMENT1A_PATH     = getExperimentPath(EXPERIMENT1A_XPI_NAME);
const EXPERIMENT1A_XPI_SHA1 = "sha1:" + sha1File(EXPERIMENT1A_PATH);

const EXPERIMENT2_ID       = "test-experiment-2@tests.mozilla.org"
const EXPERIMENT2_XPI_NAME = "experiment-2.xpi";
const EXPERIMENT2_PATH     = getExperimentPath(EXPERIMENT2_XPI_NAME);
const EXPERIMENT2_XPI_SHA1 = "sha1:" + sha1File(EXPERIMENT2_PATH);

const EXPERIMENT3_ID       = "test-experiment-3@tests.mozilla.org";
const EXPERIMENT4_ID       = "test-experiment-4@tests.mozilla.org";

const DEFAULT_BUILDID      = "2014060601";

const FAKE_EXPERIMENTS_1 = [
  {
    id: "id1",
    name: "experiment1",
    description: "experiment 1",
    active: true,
    detailUrl: "https://dummy/experiment1",
  },
];

const FAKE_EXPERIMENTS_2 = [
  {
    id: "id2",
    name: "experiment2",
    description: "experiment 2",
    active: false,
    endDate: new Date(2014, 2, 11, 2, 4, 35, 42).getTime(),
    detailUrl: "https://dummy/experiment2",
  },
  {
    id: "id1",
    name: "experiment1",
    description: "experiment 1",
    active: false,
    endDate: new Date(2014, 2, 10, 0, 0, 0, 0).getTime(),
    detailURL: "https://dummy/experiment1",
  },
];

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

let gGlobalScope = this;
function loadAddonManager() {
  let ns = {};
  Cu.import("resource://gre/modules/Services.jsm", ns);
  let head = "../../../../toolkit/mozapps/extensions/test/xpcshell/head_addons.js";
  let file = do_get_file(head);
  let uri = ns.Services.io.newFileURI(file);
  ns.Services.scriptloader.loadSubScript(uri.spec, gGlobalScope);
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
  startupManager();
}

function getExperimentAddons() {
  let deferred = Promise.defer();

  AddonManager.getAddonsByTypes(["experiment"], deferred.resolve);

  return deferred.promise;
}

function createAppInfo(optionsIn) {
  const XULAPPINFO_CONTRACTID = "@mozilla.org/xre/app-info;1";
  const XULAPPINFO_CID = Components.ID("{c763b610-9d49-455a-bbd2-ede71682a1ac}");

  let options = optionsIn || {};
  let id = options.id || "xpcshell@tests.mozilla.org";
  let name = options.name || "XPCShell";
  let version = options.version || "1.0";
  let platformVersion = options.platformVersion || "1.0";
  let date = options.date || new Date();

  let buildID = options.buildID || DEFAULT_BUILDID;

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

/**
 * Replace the experiments on an Experiments with a new list.
 *
 * This monkeypatches getExperiments(). It doesn't monkeypatch the internal
 * experiments list. So its utility is not as great as it could be.
 */
function replaceExperiments(experiment, list) {
  Object.defineProperty(experiment, "getExperiments", {
    writable: true,
    value: () => {
      return Promise.resolve(list);
    },
  });
}
