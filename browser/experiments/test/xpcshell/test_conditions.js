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
  do_register_cleanup(() => gReporter._shutdown());

  patchPolicy(gPolicy, {
    updatechannel: () => "nightly",
    locale: () => "en-US",
    healthReportPayload: () => Promise.resolve(payload),
    random: () => 0.5,
  });

  Services.prefs.setBoolPref(PREF_EXPERIMENTS_ENABLED, true);
  Services.prefs.setIntPref(PREF_LOGGING_LEVEL, 0);
  Services.prefs.setBoolPref(PREF_LOGGING_DUMP, true);

  let experiments = new Experiments.Experiments();
});

function arraysEqual(a, b) {
  if (a.length !== b.length) {
    return false;
  }

  for (let i=0; i<a.length; ++i) {
    if (a[i] !== b[i]) {
      return false;
    }
  }

  return true;
}

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

    // misc. environment

    [false, ["appName"], {appName: []}],
    [false, ["appName"], {appName: ["foo", gAppInfo.name + "-invalid"]}],
    [true,  null,        {appName: ["not-an-app-name", gAppInfo.name]}],

    [false, ["os"], {os: []}],
    [false, ["os"], {os: ["42", "abcdef"]}],
    [true,  null,   {os: [gAppInfo.OS, "plan9"]}],

    [false, ["channel"], {channel: []}],
    [false, ["channel"], {channel: ["foo", gPolicy.updatechannel() + "-invalid"]}],
    [true,  null,        {channel: ["not-a-channel", gPolicy.updatechannel()]}],

    [false, ["locale"], {locale: []}],
    [false, ["locale"], {locale: ["foo", gPolicy.locale + "-invalid"]}],
    [true,  null,       {locale: ["not-a-locale", gPolicy.locale()]}],

    // version

    [false, ["version"], {version: []}],
    [false, ["version"], {version: ["-1", gAppInfo.version + "-invalid", "asdf", "0,4", "99.99", "0.1.1.1"]}],
    [true,  null,        {version: ["99999999.999", "-1", gAppInfo.version]}],

    [false, ["minVersion"], {minVersion: "1.0.1"}],
    [true,  null,           {minVersion: "1.0b1"}],
    [true,  null,           {minVersion: "1.0"}],
    [true,  null,           {minVersion: "0.9"}],

    [false, ["maxVersion"], {maxVersion: "0.1"}],
    [false, ["maxVersion"], {maxVersion: "0.9.9"}],
    [false, ["maxVersion"], {maxVersion: "1.0b1"}],
    [true,  ["maxVersion"], {maxVersion: "1.0"}],
    [true,  ["maxVersion"], {maxVersion: "1.7pre"}],

    // build id

    [false, ["buildIDs"], {buildIDs: []}],
    [false, ["buildIDs"], {buildIDs: ["not-a-build-id", gAppInfo.platformBuildID + "-invalid"]}],
    [true,  null,         {buildIDs: ["not-a-build-id", gAppInfo.platformBuildID]}],

    [true,  null,           {minBuildID: "2014060501"}],
    [true,  null,           {minBuildID: "2014060601"}],
    [false, ["minBuildID"], {minBuildID: "2014060701"}],

    [false, ["maxBuildID"], {maxBuildID: "2014010101"}],
    [true,  null,           {maxBuildID: "2014060601"}],
    [true,  null,           {maxBuildID: "2014060901"}],

    // sample

    [false, ["sample"], {sample: -1 }],
    [false, ["sample"], {sample: 0.0}],
    [false, ["sample"], {sample: 0.1}],
    [true,  null,       {sample: 0.5}],
    [true,  null,       {sample: 0.6}],
    [true,  null,       {sample: 1.0}],
    [true,  null,       {sample: 0.5}],

    // experiment control

    [false, ["disabled"], {disabled: true}],
    [true,  null,         {disabled: false}],

    [false, ["frozen"], {frozen: true}],
    [true,  null,       {frozen: false}],

    [false, null, {frozen: true,  disabled: true}],
    [false, null, {frozen: true,  disabled: false}],
    [false, null, {frozen: false, disabled: true}],
    [true,  null, {frozen: false, disabled: false}],

    // jsfilter

    [true,  null, {jsfilter: "function filter(c) { return true; }"}],
    [false, ["jsfilter-false"], {jsfilter: "function filter(c) { return false; }"}],
    [true,  null, {jsfilter: "function filter(c) { return 123; }"}], // truthy
    [false, ["jsfilter-false"], {jsfilter: "function filter(c) { return ''; }"}], // falsy
    [false, ["jsfilter-false"], {jsfilter: "function filter(c) { var a = []; }"}], // undefined
    [false, ["jsfilter-threw", "some error"], {jsfilter: "function filter(c) { throw new Error('some error'); }"}],
    [false, ["jsfilter-evalfailed"], {jsfilter: "123, this won't work"}],
    [true,  null, {jsfilter: "var filter = " + sanityFilter.toSource()}],
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

    let expectedReason = entry[1];
    if (!applicable && expectedReason) {
      Assert.ok(arraysEqual(reason, expectedReason),
        "Experiment rejection reasons should match for test " + i + ". "
        + "Got " + JSON.stringify(reason) + ", expected "
        + JSON.stringify(expectedReason));
    }
  }
});

add_task(function* test_times() {
  let now = new Date(2014, 5, 6, 12);
  let nowSec = now.getTime() / 1000;
  let testData = [
    // "expected applicable?", rejection reason or null, fake now date, manifest data

    // start time

    [true,  null, now,
      {startTime: nowSec -  5 * SEC_IN_ONE_DAY,
         endTime: nowSec + 10 * SEC_IN_ONE_DAY}],
    [true,  null, now,
      {startTime: nowSec ,
         endTime: nowSec + 10 * SEC_IN_ONE_DAY}],
    [false,  "startTime", now,
      {startTime: nowSec +  5 * SEC_IN_ONE_DAY,
         endTime: nowSec + 10 * SEC_IN_ONE_DAY}],

    // end time

    [false,  "endTime", now,
      {startTime: nowSec -  5 * SEC_IN_ONE_DAY,
         endTime: nowSec - 10 * SEC_IN_ONE_DAY}],
    [false,  "endTime", now,
      {startTime: nowSec -  5 * SEC_IN_ONE_DAY,
         endTime: nowSec -  5 * SEC_IN_ONE_DAY}],

    // max start time

    [false,  "maxStartTime", now,
      {maxStartTime: nowSec - 15 * SEC_IN_ONE_DAY,
          startTime: nowSec - 10 * SEC_IN_ONE_DAY,
            endTime: nowSec + 10 * SEC_IN_ONE_DAY}],
    [false,  "maxStartTime", now,
      {maxStartTime: nowSec -  1 * SEC_IN_ONE_DAY,
          startTime: nowSec - 10 * SEC_IN_ONE_DAY,
            endTime: nowSec + 10 * SEC_IN_ONE_DAY}],
    [false,  "maxStartTime", now,
      {maxStartTime: nowSec - 10 * SEC_IN_ONE_DAY,
          startTime: nowSec - 10 * SEC_IN_ONE_DAY,
            endTime: nowSec + 10 * SEC_IN_ONE_DAY}],
    [true,  null, now,
      {maxStartTime: nowSec,
          startTime: nowSec - 10 * SEC_IN_ONE_DAY,
            endTime: nowSec + 10 * SEC_IN_ONE_DAY}],
    [true,  null, now,
      {maxStartTime: nowSec +  1 * SEC_IN_ONE_DAY,
          startTime: nowSec - 10 * SEC_IN_ONE_DAY,
            endTime: nowSec + 10 * SEC_IN_ONE_DAY}],

    // max active seconds

    [true,  null, now,
      {maxActiveSeconds:           5 * SEC_IN_ONE_DAY,
              startTime: nowSec - 10 * SEC_IN_ONE_DAY,
                endTime: nowSec + 10 * SEC_IN_ONE_DAY}],
    [true,  null, now,
      {maxActiveSeconds:          10 * SEC_IN_ONE_DAY,
              startTime: nowSec - 10 * SEC_IN_ONE_DAY,
                endTime: nowSec + 10 * SEC_IN_ONE_DAY}],
    [true,  null, now,
      {maxActiveSeconds:          15 * SEC_IN_ONE_DAY,
              startTime: nowSec - 10 * SEC_IN_ONE_DAY,
                endTime: nowSec + 10 * SEC_IN_ONE_DAY}],
    [true,  null, now,
      {maxActiveSeconds:          20 * SEC_IN_ONE_DAY,
              startTime: nowSec - 10 * SEC_IN_ONE_DAY,
                endTime: nowSec + 10 * SEC_IN_ONE_DAY}],
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
