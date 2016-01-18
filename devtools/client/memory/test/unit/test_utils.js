/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the task creator `takeSnapshotAndCensus()` for the whole flow of
 * taking a snapshot, and its sub-actions. Tests the formatNumber and
 * formatPercent methods.
 */

let utils = require("devtools/client/memory/utils");
let { snapshotState: states, breakdowns } = require("devtools/client/memory/constants");
let { Preferences } = require("resource://gre/modules/Preferences.jsm");

function run_test() {
  run_next_test();
}

add_task(function *() {
  ok(utils.breakdownEquals(breakdowns.allocationStack.breakdown, {
    by: "allocationStack",
    then: { by: "count", count: true, bytes: true },
    noStack: { by: "count", count: true, bytes: true },
  }), "utils.breakdownEquals() passes with preset"),

  ok(!utils.breakdownEquals(breakdowns.allocationStack.breakdown, {
    by: "allocationStack",
    then: { by: "count", count: false, bytes: true },
    noStack: { by: "count", count: true, bytes: true },
  }), "utils.breakdownEquals() fails when deep properties do not match");

  ok(!utils.breakdownEquals(breakdowns.allocationStack.breakdown, {
    by: "allocationStack",
    then: { by: "count", bytes: true },
    noStack: { by: "count", count: true, bytes: true },
  }), "utils.breakdownEquals() fails when deep properties are missing.");

  let s1 = utils.createSnapshot({});
  let s2 = utils.createSnapshot({});
  equal(s1.state, states.SAVING, "utils.createSnapshot() creates snapshot in saving state");
  ok(s1.id !== s2.id, "utils.createSnapshot() creates snapshot with unique ids");

  ok(utils.breakdownEquals(utils.breakdownNameToSpec("coarseType"), breakdowns.coarseType.breakdown),
    "utils.breakdownNameToSpec() works for presets");
  ok(utils.breakdownEquals(utils.breakdownNameToSpec("coarseType"), breakdowns.coarseType.breakdown),
    "utils.breakdownNameToSpec() works for presets");

  let custom = { by: "internalType", then: { by: "count", bytes: true }};
  Preferences.set("devtools.memory.custom-breakdowns", JSON.stringify({ "My Breakdown": custom }));

  ok(utils.breakdownEquals(utils.getCustomBreakdowns()["My Breakdown"], custom),
    "utils.getCustomBreakdowns() returns custom breakdowns");

  ok(true, "test formatNumber util functions");
  equal(utils.formatNumber(12), "12", "formatNumber returns 12 for 12");

  equal(utils.formatNumber(0), "0", "formatNumber returns 0 for 0");
  equal(utils.formatNumber(-0), "0", "formatNumber returns 0 for -0");
  equal(utils.formatNumber(+0), "0", "formatNumber returns 0 for +0");

  equal(utils.formatNumber(1234567), "1 234 567",
    "formatNumber adds a space every 3rd digit");
  equal(utils.formatNumber(12345678), "12 345 678",
    "formatNumber adds a space every 3rd digit");
  equal(utils.formatNumber(123456789), "123 456 789",
    "formatNumber adds a space every 3rd digit");

  equal(utils.formatNumber(12, true), "+12",
    "formatNumber can display number sign");
  equal(utils.formatNumber(-12, true), "-12",
    "formatNumber can display number sign (negative)");

  ok(true, "test formatPercent util functions");
  equal(utils.formatPercent(12), "12%", "formatPercent returns 12% for 12");
  equal(utils.formatPercent(12345), "12 345%",
    "formatPercent returns 12 345% for 12345");
});
