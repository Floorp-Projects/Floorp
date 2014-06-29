/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";
Cu.import("resource:///modules/experiments/Experiments.jsm");

const SEC_IN_ONE_DAY  = 24 * 60 * 60;
const MS_IN_ONE_DAY   = SEC_IN_ONE_DAY * 1000;

let cacheData = {
  _enabled: true,
  _manifestData: {
    id: "foobartestid",
    xpiURL: "http://example.com/foo.xpi",
    xpiHash: "sha256:abcde",
    startTime: 0,
    endTime: 2000000000,
    maxActiveSeconds: 40000000,
    appName: "TestApp",
    channel: "test-foo",
  },
  _needsUpdate: false,
  _randomValue: 0.5,
  _failedStart: false,
  _name: "Foo",
  _description: "Foobar",
  _homepageURL: "",
  _addonId: "foo@test",
  _startDate: 0,
  _endDate: 2000000000,
  _branch: null
};

add_task(function* test_valid() {
  let e = new Experiments.ExperimentEntry();
  Assert.ok(e.initFromCacheData(cacheData));
  Assert.ok(e.enabled);
});

add_task(function* test_upgrade() {
  let e = new Experiments.ExperimentEntry();
  delete cacheData._branch;
  Assert.ok(e.initFromCacheData(cacheData));
  Assert.ok(e.enabled);
});

add_task(function* test_missing() {
  let e = new Experiments.ExperimentEntry();
  delete cacheData._name;
  Assert.ok(!e.initFromCacheData(cacheData));
});

function run_test() {
  run_next_test();
}
