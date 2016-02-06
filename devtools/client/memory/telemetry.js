/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This module exports methods to record telemetry data for memory tool usage.
//
// NB: Ensure that *every* exported function is wrapped in `makeInfallible` so
// that our probes don't accidentally break code that actually does productive
// work for the user!

const { telemetry } = require("Services");
const { makeInfallible, immutableUpdate } = require("devtools/shared/DevToolsUtils");
const { dominatorTreeBreakdowns, breakdowns } = require("./constants");

exports.countTakeSnapshot = makeInfallible(function () {
  const histogram = telemetry.getHistogramById("DEVTOOLS_MEMORY_TAKE_SNAPSHOT_COUNT");
  histogram.add(1);
}, "devtools/client/memory/telemetry#countTakeSnapshot");

exports.countImportSnapshot = makeInfallible(function () {
  const histogram = telemetry.getHistogramById("DEVTOOLS_MEMORY_IMPORT_SNAPSHOT_COUNT");
  histogram.add(1);
}, "devtools/client/memory/telemetry#countImportSnapshot");

exports.countExportSnapshot = makeInfallible(function () {
  const histogram = telemetry.getHistogramById("DEVTOOLS_MEMORY_EXPORT_SNAPSHOT_COUNT");
  histogram.add(1);
}, "devtools/client/memory/telemetry#countExportSnapshot");

const COARSE_TYPE = "Coarse Type";
const ALLOCATION_STACK = "Allocation Stack";
const OBJECT_CLASS = "Object Class";
const INTERNAL_TYPE = "Internal Type";
const CUSTOM = "Custom";

/**
 * @param {Boolean} inverted
 *        True if the census was inverted, false otherwise.
 *
 * @param {String|null} filter
 *        The filter string used, if any.
 *
 * @param {Boolean} diffing
 *        True if the census was a diffing census, false otherwise.
 *
 * @param {Object} breakdown
 *        The breakdown used with the census.
 */
exports.countCensus = makeInfallible(function ({ inverted, filter, diffing, breakdown }) {
  let histogram = telemetry.getHistogramById("DEVTOOLS_MEMORY_INVERTED_CENSUS");
  histogram.add(!!inverted);

  histogram = telemetry.getHistogramById("DEVTOOLS_MEMORY_FILTER_CENSUS");
  histogram.add(!!filter);

  histogram = telemetry.getHistogramById("DEVTOOLS_MEMORY_DIFF_CENSUS");
  histogram.add(!!diffing);

  histogram = telemetry.getKeyedHistogramById("DEVTOOLS_MEMORY_BREAKDOWN_CENSUS_COUNT");
  if (breakdown === breakdowns.coarseType.breakdown) {
    histogram.add(COARSE_TYPE);
  } else if (breakdown === breakdowns.allocationStack.breakdown) {
    histogram.add(ALLOCATION_STACK);
  } else if (breakdown === breakdowns.objectClass.breakdown) {
    histogram.add(OBJECT_CLASS);
  } else if (breakdown === breakdowns.internalType.breakdown) {
    histogram.add(INTERNAL_TYPE);
  } else {
    histogram.add(CUSTOM);
  }
}, "devtools/client/memory/telemetry#countCensus");

/**
 * @param {Object} opts
 *        The same parameters specified for countCensus.
 */
exports.countDiff = makeInfallible(function (opts) {
  exports.countCensus(immutableUpdate(opts, { diffing: true }));
}, "devtools/client/memory/telemetry#countDiff");

/**
 * @param {Object} breakdown
 *        The breakdown used to label nodes in the dominator tree.
 */
exports.countDominatorTree = makeInfallible(function ({ breakdown }) {
  let histogram = telemetry.getHistogramById("DEVTOOLS_MEMORY_DOMINATOR_TREE_COUNT");
  histogram.add(1);

  histogram = telemetry.getKeyedHistogramById("DEVTOOLS_MEMORY_BREAKDOWN_DOMINATOR_TREE_COUNT");
  if (breakdown === dominatorTreeBreakdowns.coarseType.breakdown) {
    histogram.add(COARSE_TYPE);
  } else if (breakdown === dominatorTreeBreakdowns.allocationStack.breakdown) {
    histogram.add(ALLOCATION_STACK);
  } else if (breakdown === dominatorTreeBreakdowns.internalType.breakdown) {
    histogram.add(INTERNAL_TYPE);
  } else {
    histogram.add(CUSTOM);
  }
}, "devtools/client/memory/telemetry#countDominatorTree");
