/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the task creator `takeSnapshotAndCensus()` for the whole flow of
 * taking a snapshot, and its sub-actions. Tests the formatNumber and
 * formatPercent methods.
 */

let utils = require("devtools/client/memory/utils");
let { snapshotState: states, viewState } = require("devtools/client/memory/constants");
let { Preferences } = require("resource://gre/modules/Preferences.jsm");

add_task(async function() {
  let s1 = utils.createSnapshot({ view: { state: viewState.CENSUS } });
  let s2 = utils.createSnapshot({ view: { state: viewState.CENSUS } });
  equal(s1.state, states.SAVING,
        "utils.createSnapshot() creates snapshot in saving state");
  ok(s1.id !== s2.id, "utils.createSnapshot() creates snapshot with unique ids");

  let custom = { by: "internalType", then: { by: "count", bytes: true }};
  Preferences.set("devtools.memory.custom-census-displays",
                  JSON.stringify({ "My Display": custom }));

  equal(utils.getCustomCensusDisplays()["My Display"].by, custom.by,
        "utils.getCustomCensusDisplays() returns custom displays");

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

  equal(utils.formatAbbreviatedBytes(12), "12B", "Formats bytes");
  equal(utils.formatAbbreviatedBytes(12345), "12KiB", "Formats kilobytes");
  equal(utils.formatAbbreviatedBytes(12345678), "11MiB", "Formats megabytes");
  equal(utils.formatAbbreviatedBytes(12345678912), "11GiB", "Formats gigabytes");

  equal(utils.hslToStyle(0.5, 0.6, 0.7),
    "hsl(180,60%,70%)", "hslToStyle converts an array to a style string");
  equal(utils.hslToStyle(0, 0, 0),
    "hsl(0,0%,0%)", "hslToStyle converts an array to a style string");
  equal(utils.hslToStyle(1, 1, 1),
    "hsl(360,100%,100%)", "hslToStyle converts an array to a style string");

  equal(utils.lerp(5, 7, 0), 5, "lerp return first number for 0");
  equal(utils.lerp(5, 7, 1), 7, "lerp return second number for 1");
  equal(utils.lerp(5, 7, 0.5), 6, "lerp interpolates the numbers for 0.5");
});
