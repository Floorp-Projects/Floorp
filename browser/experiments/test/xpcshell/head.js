/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* exported PREF_EXPERIMENTS_ENABLED, PREF_LOGGING_LEVEL, PREF_LOGGING_DUMP
            PREF_MANIFEST_URI, PREF_FETCHINTERVAL, EXPERIMENT1_ID,
            EXPERIMENT1_NAME, EXPERIMENT1_XPI_SHA1, EXPERIMENT1A_NAME,
            EXPERIMENT1A_XPI_SHA1, EXPERIMENT2_ID, EXPERIMENT2_XPI_SHA1,
            EXPERIMENT3_ID, EXPERIMENT4_ID, FAKE_EXPERIMENTS_1,
            FAKE_EXPERIMENTS_2, gAppInfo, removeCacheFile, defineNow,
            futureDate, dateToSeconds, loadAddonManager, promiseRestartManager,
            startAddonManagerOnly, getExperimentAddons, replaceExperiments */

var {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://testing-common/AddonManagerTesting.jsm");
Cu.import("resource://testing-common/AddonTestUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
                                  "resource://gre/modules/AddonManager.jsm");

const PREF_EXPERIMENTS_ENABLED  = "experiments.enabled";
const PREF_LOGGING_LEVEL        = "experiments.logging.level";
const PREF_LOGGING_DUMP         = "experiments.logging.dump";
const PREF_MANIFEST_URI         = "experiments.manifest.uri";
const PREF_FETCHINTERVAL        = "experiments.manifest.fetchIntervalSeconds";
const PREF_TELEMETRY_ENABLED    = "toolkit.telemetry.enabled";

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

  let rv = "";
  for (let i = 0; i < bytes.length; i++) {
    rv += ("0" + bytes.charCodeAt(i).toString(16)).substr(-2);
  }
  return rv;
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

const FAKE_EXPERIMENTS_1 = [
  {
    id: "id1",
    name: "experiment1",
    description: "experiment 1",
    active: true,
    detailUrl: "https://dummy/experiment1",
    branch: "foo",
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
    branch: null,
  },
  {
    id: "id1",
    name: "experiment1",
    description: "experiment 1",
    active: false,
    endDate: new Date(2014, 2, 10, 0, 0, 0, 0).getTime(),
    detailURL: "https://dummy/experiment1",
    branch: null,
  },
];

var gAppInfo = null;

function removeCacheFile() {
  let path = OS.Path.join(OS.Constants.Path.profileDir, "experiments.json");
  return OS.File.remove(path);
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

var gGlobalScope = this;
function loadAddonManager() {
  AddonTestUtils.init(gGlobalScope);
  AddonTestUtils.overrideCertDB();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
  return AddonTestUtils.promiseStartupManager();
}

const {
  promiseRestartManager,
} = AddonTestUtils;

// Starts the addon manager without creating app info. We can't directly use
// |loadAddonManager| defined above in test_conditions.js as it would make the test fail.
function startAddonManagerOnly() {
  let addonManager = Cc["@mozilla.org/addons/integration;1"]
                       .getService(Ci.nsIObserver)
                       .QueryInterface(Ci.nsITimerCallback);
  addonManager.observe(null, "addons-startup", null);
}

function getExperimentAddons(previous = false) {
  return new Promise(resolve => {

    AddonManager.getAddonsByTypes(["experiment"], (addons) => {
      if (previous) {
        resolve(addons);
      } else {
        resolve(addons.filter(a => !a.appDisabled));
      }
    });

  });
}

function createAppInfo(ID = "xpcshell@tests.mozilla.org", name = "XPCShell",
                       version = "1.0", platformVersion = "1.0") {
  AddonTestUtils.createAppInfo(ID, name, version, platformVersion);
  gAppInfo = AddonTestUtils.appInfo;
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

// Experiments require Telemetry to be enabled, and that's not true for debug
// builds. Let's just enable it here instead of going through each test.
Services.prefs.setBoolPref(PREF_TELEMETRY_ENABLED, true);
