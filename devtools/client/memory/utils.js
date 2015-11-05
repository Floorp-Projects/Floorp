/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Cu } = require("chrome");

Cu.import("resource://devtools/client/shared/widgets/ViewHelpers.jsm");
const STRINGS_URI = "chrome://devtools/locale/memory.properties"
const L10N = exports.L10N = new ViewHelpers.L10N(STRINGS_URI);

const { URL } = require("sdk/url");
const { assert } = require("devtools/shared/DevToolsUtils");
const { Preferences } = require("resource://gre/modules/Preferences.jsm");
const CUSTOM_BREAKDOWN_PREF = "devtools.memory.custom-breakdowns";
const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const { snapshotState: states, breakdowns } = require("./constants");

/**
 * Takes a snapshot object and returns the
 * localized form of its timestamp to be used as a title.
 *
 * @param {Snapshot} snapshot
 * @return {String}
 */
exports.getSnapshotTitle = function (snapshot) {
  if (!snapshot.creationTime) {
    return L10N.getStr("snapshot-title.loading");
  }

  let date = new Date(snapshot.creationTime / 1000);
  return date.toLocaleTimeString(void 0, {
    year: "2-digit",
    month: "2-digit",
    day: "2-digit",
    hour12: false
  });
};

/**
 * Returns an array of objects with the unique key `name`
 * and `displayName` for each breakdown.
 *
 * @return {Object{name, displayName}}
 */
exports.getBreakdownDisplayData = function () {
  return exports.getBreakdownNames().map(name => {
    // If it's a preset use the display name value
    let preset = breakdowns[name];
    let displayName = name;
    if (preset && preset.displayName) {
      displayName = preset.displayName;
    }
    return { name, displayName };
  });
};

/**
 * Returns an array of the unique names for each breakdown in
 * presets and custom pref.
 *
 * @return {Array<Breakdown>}
 */
exports.getBreakdownNames = function () {
  let custom = exports.getCustomBreakdowns();
  return Object.keys(Object.assign({}, breakdowns, custom));
};

/**
 * Returns custom breakdowns defined in `devtools.memory.custom-breakdowns` pref.
 *
 * @return {Object}
 */
exports.getCustomBreakdowns = function () {
  let customBreakdowns = Object.create(null);
  try {
    customBreakdowns = JSON.parse(Preferences.get(CUSTOM_BREAKDOWN_PREF)) || Object.create(null);
  } catch (e) {
    DevToolsUtils.reportException(
      `String stored in "${CUSTOM_BREAKDOWN_PREF}" pref cannot be parsed by \`JSON.parse()\`.`);
  }
  return customBreakdowns;
}

/**
 * Converts a breakdown preset name, like "allocationStack", and returns the
 * spec for the breakdown. Also checks properties of keys in the `devtools.memory.custom-breakdowns`
 * pref. If not found, returns an empty object.
 *
 * @param {String} name
 * @return {Object}
 */

exports.breakdownNameToSpec = function (name) {
  let customBreakdowns = exports.getCustomBreakdowns();

  // If breakdown is already a breakdown, use it
  if (typeof name === "object") {
    return name;
  }
  // If it's in our custom breakdowns, use it
  else if (name in customBreakdowns) {
    return customBreakdowns[name];
  }
  // If breakdown name is in our presets, use that
  else if (name in breakdowns) {
    return breakdowns[name].breakdown;
  }
  return Object.create(null);
};

/**
 * Returns a string representing a readable form of the snapshot's state.
 * More brief than `getSnapshotStatusText`.
 *
 * @param {Snapshot} snapshot
 * @return {String}
 */
exports.getSnapshotStatusText = function (snapshot) {
  assert((snapshot || {}).state,
    `Snapshot must have expected state, found ${(snapshot || {}).state}.`);

  switch (snapshot.state) {
    case states.ERROR:
      return L10N.getStr("snapshot.state.error");
    case states.SAVING:
      return L10N.getStr("snapshot.state.saving");
    case states.SAVED:
    case states.READING:
      return L10N.getStr("snapshot.state.reading");
    case states.SAVING_CENSUS:
      return L10N.getStr("snapshot.state.saving-census");
    // Both READ and SAVED_CENSUS state do not have any message
    // to show as other content will be displayed.
    case states.READ:
    case states.SAVED_CENSUS:
      return "";
  }

  assert(false, `Snapshot in unexpected state: ${snapshot.state}`);
  return "";
}

/**
 * Returns a string representing a readable form of the snapshot's state;
 * more verbose than `getSnapshotStatusText`.
 *
 * @param {Snapshot} snapshot
 * @return {String}
 */
exports.getSnapshotStatusTextFull = function (snapshot) {
  assert((snapshot || {}).state,
    `Snapshot must have expected state, found ${(snapshot || {}).state}.`);
  switch (snapshot.state) {
    case states.ERROR:
      return L10N.getStr("snapshot.state.error.full");
    case states.SAVING:
      return L10N.getStr("snapshot.state.saving.full");
    case states.SAVED:
    case states.READING:
      return L10N.getStr("snapshot.state.reading.full");
    case states.SAVING_CENSUS:
      return L10N.getStr("snapshot.state.saving-census.full");
    // Both READ and SAVED_CENSUS state do not have any full message
    // to show as other content will be displayed.
    case states.READ:
    case states.SAVED_CENSUS:
      return "";
  }
}

/**
 * Takes an array of snapshots and a snapshot and returns
 * the snapshot instance in `snapshots` that matches
 * the snapshot passed in.
 *
 * @param {Array<Snapshot>} snapshots
 * @param {Snapshot}
 * @return ?Snapshot
 */
exports.getSnapshot = function getSnapshot (snapshots, snapshot) {
  let found = snapshots.find(s => s.id === snapshot.id);
  if (!found) {
    DevToolsUtils.reportException(`No matching snapshot found for ${snapshot.id}`);
  }
  return found || null;
};

/**
 * Creates a new snapshot object.
 *
 * @return {Snapshot}
 */
let INC_ID = 0;
exports.createSnapshot = function createSnapshot () {
  let id = ++INC_ID;
  return {
    id,
    state: states.SAVING,
    census: null,
    path: null,
  };
};

/**
 * Takes two objects and compares them deeply, returning
 * a boolean indicating if they're equal or not. Used for breakdown
 * comparison.
 *
 * @param {Any} obj1
 * @param {Any} obj2
 * @return {Boolean}
 */
exports.breakdownEquals = function (obj1, obj2) {
  let type1 = typeof obj1;
  let type2 = typeof obj2;

  // Quick checks
  if (type1 !== type2 || (Array.isArray(obj1) !== Array.isArray(obj2))) {
    return false;
  }

  if (obj1 === obj2) {
    return true;
  }

  if (Array.isArray(obj1)) {
    if (obj1.length !== obj2.length) { return false; }
    return obj1.every((_, i) => exports.breakdownEquals(obj[1], obj2[i]));
  }
  else if (type1 === "object") {
    let k1 = Object.keys(obj1);
    let k2 = Object.keys(obj2);

    if (k1.length !== k2.length) {
      return false;
    }

    return k1.every(k => exports.breakdownEquals(obj1[k], obj2[k]));
  }

  return false;
};

/**
 * Takes a snapshot and returns the total bytes and
 * total count that this snapshot represents.
 *
 * @param {Snapshot} snapshot
 * @return {Object}
 */
exports.getSnapshotTotals = function (snapshot) {
  let bytes, count;

  let census = snapshot.census;

  if (snapshot.inverted) {
    while (census) {
      bytes = census.totalBytes;
      count = census.totalCount;
      census = census.children && census.children[0];
    }
  } else {
    if (census) {
      bytes = census.totalBytes;
      count = census.totalCount;
    }
  }

  return {
    bytes: bytes || 0,
    count: count || 0,
  };
};

/**
 * Parse a source into a short and long name as well as a host name.
 *
 * @param {String} source
 *        The source to parse.
 *
 * @returns {Object}
 *          An object with the following properties:
 *            - {String} short: A short name for the source.
 *            - {String} long: The full, long name for the source.
 *            - {String?} host: If available, the host name for the source.
 */
exports.parseSource = function (source) {
  const sourceStr = source ? String(source) : "";

  let short;
  let long;
  let host;

  try {
    const url = new URL(sourceStr);
    short = url.fileName;
    host = url.host;
    long = url.toString();
  } catch (e) {
    // Malformed URI.
    long = sourceStr;
    short = sourceStr.slice(0, 100);
  }

  if (!short) {
    // Last ditch effort.

    if (!long) {
      long = L10N.getStr("unknownSource");
    }

    short = long.slice(0, 100);
  }

  return { short, long, host };
};
