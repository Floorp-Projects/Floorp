/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Cu, Cc, Ci } = require("chrome");

Cu.import("resource://devtools/client/shared/widgets/ViewHelpers.jsm");
const STRINGS_URI = "chrome://devtools/locale/memory.properties"
const L10N = exports.L10N = new ViewHelpers.L10N(STRINGS_URI);

const { URL } = require("sdk/url");
const { OS } = require("resource://gre/modules/osfile.jsm");
const { assert } = require("devtools/shared/DevToolsUtils");
const { Preferences } = require("resource://gre/modules/Preferences.jsm");
const CUSTOM_BREAKDOWN_PREF = "devtools.memory.custom-breakdowns";
const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const { snapshotState: states, diffingState, breakdowns } = require("./constants");

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

  if (snapshot.imported) {
    // Strip out the extension if it's the expected ".fxsnapshot"
    return OS.Path.basename(snapshot.path.replace(/\.fxsnapshot$/, ""));
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
 * Returns a string representing a readable form of the snapshot's state. More
 * concise than `getStatusTextFull`.
 *
 * @param {snapshotState | diffingState} state
 * @return {String}
 */
exports.getStatusText = function (state) {
  assert(state, "Must have a state");

  switch (state) {
    case diffingState.ERROR:
      return L10N.getStr("diffing.state.error");

    case states.ERROR:
      return L10N.getStr("snapshot.state.error");

    case states.SAVING:
      return L10N.getStr("snapshot.state.saving");

    case states.IMPORTING:
      return L10N.getStr("snapshot.state.importing");

    case states.SAVED:
    case states.READING:
      return L10N.getStr("snapshot.state.reading");

    case states.SAVING_CENSUS:
      return L10N.getStr("snapshot.state.saving-census");

    case diffingState.TAKING_DIFF:
      return L10N.getStr("diffing.state.taking-diff");

    case diffingState.SELECTING:
      return L10N.getStr("diffing.state.selecting");

    // These states do not have any message to show as other content will be
    // displayed.
    case diffingState.TOOK_DIFF:
    case states.READ:
    case states.SAVED_CENSUS:
      return "";

    default:
      assert(false, `Unexpected state: ${state}`);
      return "";
  }
};

/**
 * Returns a string representing a readable form of the snapshot's state;
 * more verbose than `getStatusText`.
 *
 * @param {snapshotState | diffingState} state
 * @return {String}
 */
exports.getStatusTextFull = function (state) {
  assert(!!state, "Must have a state");

  switch (state) {
    case diffingState.ERROR:
      return L10N.getStr("diffing.state.error.full");

    case states.ERROR:
      return L10N.getStr("snapshot.state.error.full");

    case states.SAVING:
      return L10N.getStr("snapshot.state.saving.full");

    case states.IMPORTING:
      return L10N.getStr("snapshot.state.importing");

    case states.SAVED:
    case states.READING:
      return L10N.getStr("snapshot.state.reading.full");

    case states.SAVING_CENSUS:
      return L10N.getStr("snapshot.state.saving-census.full");

    case diffingState.TAKING_DIFF:
      return L10N.getStr("diffing.state.taking-diff.full");

    case diffingState.SELECTING:
      return L10N.getStr("diffing.state.selecting.full");

    // These states do not have any full message to show as other content will
    // be displayed.
    case diffingState.TOOK_DIFF:
    case states.READ:
    case states.SAVED_CENSUS:
      return "";

    default:
      assert(false, `Unexpected state: ${state}`);
      return "";
  }
};

/**
 * Return true if the snapshot is in a diffable state, false otherwise.
 *
 * @param {snapshotModel} snapshot
 * @returns {Boolean}
 */
exports.snapshotIsDiffable = function snapshotIsDiffable(snapshot) {
  return snapshot.state === states.SAVED_CENSUS
    || snapshot.state === states.SAVING_CENSUS
    || snapshot.state === states.SAVED
    || snapshot.state === states.READ;
};

/**
 * Takes an array of snapshots and a snapshot and returns
 * the snapshot instance in `snapshots` that matches
 * the snapshot passed in.
 *
 * @param {appModel} state
 * @param {snapshotId} id
 * @return {snapshotModel|null}
 */
exports.getSnapshot = function getSnapshot (state, id) {
  const found = state.snapshots.find(s => s.id === id);
  assert(found, `No matching snapshot found for ${id}`);
  return found;
};

/**
 * Creates a new snapshot object.
 *
 * @return {Snapshot}
 */
let ID_COUNTER = 0;
exports.createSnapshot = function createSnapshot() {
  return Object.freeze({
    id: ++ID_COUNTER,
    state: states.SAVING,
    census: null,
    path: null,
    imported: false,
    selected: false,
    error: null,
  });
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
const breakdownEquals = exports.breakdownEquals = function (obj1, obj2) {
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
 * Return true if the census is up to date with regards to the current
 * inversion/filtering/breakdown, false otherwise.
 *
 * @param {Boolean} inverted
 * @param {String} filter
 * @param {Object} breakdown
 * @param {censusModel} census
 *
 * @returns {Boolean}
 */
exports.censusIsUpToDate = function (inverted, filter, breakdown, census) {
  return census
      && inverted === census.inverted
      && filter === census.filter
      && breakdownEquals(breakdown, census.breakdown);
};

/**
 * Takes a snapshot and returns the total bytes and total count that this
 * snapshot represents.
 *
 * @param {CensusModel} census
 * @return {Object}
 */
exports.getSnapshotTotals = function (census) {
  let bytes = 0;
  let count = 0;

  let report = census.report;
  if (report) {
    bytes = report.totalBytes;
    count = report.totalCount;
  }

  return { bytes, count };
};

/**
 * Takes some configurations and opens up a file picker and returns
 * a promise to the chosen file if successful.
 *
 * @param {String} .title
 *        The title displayed in the file picker window.
 * @param {Array<Array<String>>} .filters
 *        An array of filters to display in the file picker. Each filter in the array
 *        is a duple of two strings, one a name for the filter, and one the filter itself
 *        (like "*.json").
 * @param {String} .defaultName
 *        The default name chosen by the file picker window.
 * @param {String} .mode
 *        The mode that this filepicker should open in. Can be "open" or "save".
 * @return {Promise<?nsILocalFile>}
 *        The file selected by the user, or null, if cancelled.
 */
exports.openFilePicker = function({ title, filters, defaultName, mode }) {
  mode = mode === "save" ? Ci.nsIFilePicker.modeSave :
         mode === "open" ? Ci.nsIFilePicker.modeOpen : null;

  if (mode == void 0) {
    throw new Error("No valid mode specified for nsIFilePicker.");
  }

  let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
  fp.init(window, title, mode);

  for (let filter of (filters || [])) {
    fp.appendFilter(filter[0], filter[1]);
  }
  fp.defaultString = defaultName;

  return new Promise(resolve => {
    fp.open({
      done: result => {
        if (result === Ci.nsIFilePicker.returnCancel) {
          resolve(null);
          return;
        }
        resolve(fp.file);
      }
    });
  });
};
