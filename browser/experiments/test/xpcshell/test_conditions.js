/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";


Cu.import("resource:///modules/experiments/Experiments.jsm");

const FILE_MANIFEST            = "experiments.manifest";
const PREF_EXPERIMENTS_ENABLED = "experiments.enabled";
const PREF_LOGGING_LEVEL       = "experiments.logging.level";
const PREF_LOGGING_DUMP        = "experiments.logging.dump";
const PREF_MANIFEST_URI        = "experiments.manifest.uri";

const SEC_IN_ONE_DAY = 24 * 60 * 60;
const MS_IN_ONE_DAY  = SEC_IN_ONE_DAY * 1000;

let gProfileDir = null;
let gHttpServer = null;
let gHttpRoot   = null;
let gReporter   = null;
let gPolicy     = null;


function ManifestEntry(data) {
  this.id = EXPERIMENT1_ID;
  this.xpiURL = "http://localhost:1/dummy.xpi";
  this.xpiHash = EXPERIMENT1_XPI_SHA1;
  this.startTime = new Date(2010, 0, 1, 12).getTime() / 1000;
  this.endTime = new Date(9001, 0, 1, 12).getTime() / 1000;
  this.maxActiveSeconds = SEC_IN_ONE_DAY;
  this.appName = ["XPCShell"];
  this.channel = ["nightly"];

  data = data || {};
  for (let k of Object.keys(data)) {
    this[k] = data[k];
  }

  if (!this.endTime) {
    this.endTime = this.startTime + 5 * SEC_IN_ONE_DAY;
  }
}

function applicableFromManifestData(data, policy) {
  let manifestData = new ManifestEntry(data);
  let entry = new Experiments.ExperimentEntry(policy);
  entry.initFromManifestData(manifestData);
  return entry.isApplicable();
}

function run_test() {
  run_next_test();
}

add_task(function* test_setup() {
  createAppInfo();
  gProfileDir = do_get_profile();
  gPolicy = new Experiments.Policy();

  gReporter = yield getReporter("json_payload_simple");
  yield gReporter.collectMeasurements();
  let payload = yield gReporter.getJSONPayload(true);

  patchPolicy(gPolicy, {
    updatechannel: () => "nightly",
    healthReportPayload: () => Promise.resolve(payload),
  });

  Services.prefs.setBoolPref(PREF_EXPERIMENTS_ENABLED, true);
  Services.prefs.setIntPref(PREF_LOGGING_LEVEL, 0);
  Services.prefs.setBoolPref(PREF_LOGGING_DUMP, true);

  let experiments = new Experiments.Experiments();
});

// This function exists solely to be .toSource()d
const sanityFilter = function filter(c) {
  if (c.telemetryPayload === undefined) {
    throw Error("No .telemetryPayload");
  }
  if (c.telemetryPayload.simpleMeasurements === undefined) {
    throw Error("No .simpleMeasurements");
  }
  if (c.healthReportPayload === undefined) {
    throw Error("No .healthReportPayload");
  }
  if (c.healthReportPayload.geckoAppInfo == undefined) {
    throw Error("No .geckoAppInfo");
  }
  return true;
}

add_task(function* test_simpleFields() {
  let testData = [
    // "expected applicable?", failure reason or null, manifest data
    [false, "os", {os: ["42", "abcdef"]}],
    [true,  null, {os: [gAppInfo.OS, "plan9"]}],
    [false, "buildIDs", {buildIDs: ["not-a-build-id", gAppInfo.platformBuildID + "-invalid"]}],
    [true,  null, {buildIDs: ["not-a-build-id", gAppInfo.platformBuildID]}],
    [true,  null, {jsfilter: "function filter(c) { return true; }"}],
    [false, null, {jsfilter: "function filter(c) { return false; }"}],
    [true,  null, {jsfilter: "function filter(c) { return 123; }"}], // truthy
    [false, null, {jsfilter: "function filter(c) { return ''; }"}], // falsy
    [false, null, {jsfilter: "function filter(c) { var a = []; }"}], // undefined
    [false, "jsfilter:rejected some error", {jsfilter: "function filter(c) { throw new Error('some error'); }"}],
    [false, "jsfilter:evalFailure", {jsfilter: "123, this won't work"}],
    [true,  {jsfilter: "var filter = " + sanityFilter.toSource()}],
  ];

  for (let i=0; i<testData.length; ++i) {
    let entry = testData[i];
    let applicable;
    let reason = null;

    yield applicableFromManifestData(entry[2], gPolicy).then(
      value => applicable = value,
      value => {
        applicable = false;
        reason = value;
      }
    );

    Assert.equal(applicable, entry[0],
      "Experiment entry applicability should match for test "
      + i + ": " + JSON.stringify(entry[2]));
    if (!applicable) {
      Assert.equal(reason, entry[1], "Experiment rejection reason should match for test " + i);
    }
  }
});

add_task(function* test_times() {
  let baseDate = new Date(2014, 5, 6, 12);
  let baseTimeSec = baseDate.getTime() / 1000;
  let testData = [
    // "expected applicable?", rejection reason or null, fake now date, manifest data
    [false, "maxStartTime", baseDate,
      {maxStartTime: baseTimeSec - 10 * SEC_IN_ONE_DAY}],
    [false, "endTime", baseDate,
      {startTime: baseTimeSec - 10 * SEC_IN_ONE_DAY,
         endTime: baseTimeSec -  5 * SEC_IN_ONE_DAY}],
    [true,  null, baseDate,
      {startTime: baseTimeSec -  5 * SEC_IN_ONE_DAY,
         endTime: baseTimeSec + 10 * SEC_IN_ONE_DAY}],
  ];

  for (let i=0; i<testData.length; ++i) {
    let entry = testData[i];
    let applicable;
    let reason = null;
    defineNow(gPolicy, entry[2]);

    yield applicableFromManifestData(entry[3], gPolicy).then(
      value => applicable = value,
      value => {
        applicable = false;
        reason = value;
      }
    );

    Assert.equal(applicable, entry[0],
      "Experiment entry applicability should match for test "
      + i + ": " + JSON.stringify([entry[2], entry[3]]));
    if (!applicable && entry[1]) {
      Assert.equal(reason, entry[1], "Experiment rejection reason should match for test " + i);
    }
  }
});

add_task(function* shutdown() {
  yield gReporter._shutdown();
  yield removeCacheFile();
});
