/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Cu, Cc, Ci } = require("chrome");

const { LocalizationHelper } = require("devtools/shared/l10n");
const STRINGS_URI = "devtools/locale/memory.properties";
const L10N = exports.L10N = new LocalizationHelper(STRINGS_URI);

const { OS } = require("resource://gre/modules/osfile.jsm");
const { assert } = require("devtools/shared/DevToolsUtils");
const { Preferences } = require("resource://gre/modules/Preferences.jsm");
const CUSTOM_CENSUS_DISPLAY_PREF = "devtools.memory.custom-census-displays";
const CUSTOM_LABEL_DISPLAY_PREF = "devtools.memory.custom-label-displays";
const CUSTOM_TREE_MAP_DISPLAY_PREF = "devtools.memory.custom-tree-map-displays";
const BYTES = 1024;
const KILOBYTES = Math.pow(BYTES, 2);
const MEGABYTES = Math.pow(BYTES, 3);
const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const {
  snapshotState: states,
  diffingState,
  censusState,
  treeMapState,
  dominatorTreeState,
  individualsState,
} = require("./constants");

/**
 * Takes a snapshot object and returns the localized form of its timestamp to be
 * used as a title.
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

function getCustomDisplaysHelper(pref) {
  let customDisplays = Object.create(null);
  try {
    customDisplays = JSON.parse(Preferences.get(pref)) || Object.create(null);
  } catch (e) {
    DevToolsUtils.reportException(
      `String stored in "${pref}" pref cannot be parsed by \`JSON.parse()\`.`);
  }
  return Object.freeze(customDisplays);
}

/**
 * Returns custom displays defined in `devtools.memory.custom-census-displays`
 * pref.
 *
 * @return {Object}
 */
exports.getCustomCensusDisplays = function () {
  return getCustomDisplaysHelper(CUSTOM_CENSUS_DISPLAY_PREF);
};

/**
 * Returns custom displays defined in
 * `devtools.memory.custom-label-displays` pref.
 *
 * @return {Object}
 */
exports.getCustomLabelDisplays = function () {
  return getCustomDisplaysHelper(CUSTOM_LABEL_DISPLAY_PREF);
};

/**
 * Returns custom displays defined in
 * `devtools.memory.custom-tree-map-displays` pref.
 *
 * @return {Object}
 */
exports.getCustomTreeMapDisplays = function () {
  return getCustomDisplaysHelper(CUSTOM_TREE_MAP_DISPLAY_PREF);
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

    case censusState.SAVING:
      return L10N.getStr("snapshot.state.saving-census");

    case treeMapState.SAVING:
      return L10N.getStr("snapshot.state.saving-tree-map");

    case diffingState.TAKING_DIFF:
      return L10N.getStr("diffing.state.taking-diff");

    case diffingState.SELECTING:
      return L10N.getStr("diffing.state.selecting");

    case dominatorTreeState.COMPUTING:
    case individualsState.COMPUTING_DOMINATOR_TREE:
      return L10N.getStr("dominatorTree.state.computing");

    case dominatorTreeState.COMPUTED:
    case dominatorTreeState.FETCHING:
      return L10N.getStr("dominatorTree.state.fetching");

    case dominatorTreeState.INCREMENTAL_FETCHING:
      return L10N.getStr("dominatorTree.state.incrementalFetching");

    case dominatorTreeState.ERROR:
      return L10N.getStr("dominatorTree.state.error");

    case individualsState.ERROR:
      return L10N.getStr("individuals.state.error");

    case individualsState.FETCHING:
      return L10N.getStr("individuals.state.fetching");

    // These states do not have any message to show as other content will be
    // displayed.
    case dominatorTreeState.LOADED:
    case diffingState.TOOK_DIFF:
    case states.READ:
    case censusState.SAVED:
    case treeMapState.SAVED:
    case individualsState.FETCHED:
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

    case censusState.SAVING:
      return L10N.getStr("snapshot.state.saving-census.full");

    case treeMapState.SAVING:
      return L10N.getStr("snapshot.state.saving-tree-map.full");

    case diffingState.TAKING_DIFF:
      return L10N.getStr("diffing.state.taking-diff.full");

    case diffingState.SELECTING:
      return L10N.getStr("diffing.state.selecting.full");

    case dominatorTreeState.COMPUTING:
    case individualsState.COMPUTING_DOMINATOR_TREE:
      return L10N.getStr("dominatorTree.state.computing.full");

    case dominatorTreeState.COMPUTED:
    case dominatorTreeState.FETCHING:
      return L10N.getStr("dominatorTree.state.fetching.full");

    case dominatorTreeState.INCREMENTAL_FETCHING:
      return L10N.getStr("dominatorTree.state.incrementalFetching.full");

    case dominatorTreeState.ERROR:
      return L10N.getStr("dominatorTree.state.error.full");

    case individualsState.ERROR:
      return L10N.getStr("individuals.state.error.full");

    case individualsState.FETCHING:
      return L10N.getStr("individuals.state.fetching.full");

    // These states do not have any full message to show as other content will
    // be displayed.
    case dominatorTreeState.LOADED:
    case diffingState.TOOK_DIFF:
    case states.READ:
    case censusState.SAVED:
    case treeMapState.SAVED:
    case individualsState.FETCHED:
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
  return (snapshot.census && snapshot.census.state === censusState.SAVED)
    || (snapshot.census && snapshot.census.state === censusState.SAVING)
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
exports.getSnapshot = function getSnapshot(state, id) {
  const found = state.snapshots.find(s => s.id === id);
  assert(found, `No matching snapshot found with id = ${id}`);
  return found;
};

/**
 * Get the ID of the selected snapshot, if one is selected, null otherwise.
 *
 * @returns {SnapshotId|null}
 */
exports.findSelectedSnapshot = function (state) {
  const found = state.snapshots.find(s => s.selected);
  return found ? found.id : null;
};

/**
 * Creates a new snapshot object.
 *
 * @param {appModel} state
 * @return {Snapshot}
 */
let ID_COUNTER = 0;
exports.createSnapshot = function createSnapshot(state) {
  let dominatorTree = null;
  if (state.view.state === dominatorTreeState.DOMINATOR_TREE) {
    dominatorTree = Object.freeze({
      dominatorTreeId: null,
      root: null,
      error: null,
      state: dominatorTreeState.COMPUTING,
    });
  }

  return Object.freeze({
    id: ++ID_COUNTER,
    state: states.SAVING,
    dominatorTree,
    census: null,
    treeMap: null,
    path: null,
    imported: false,
    selected: false,
    error: null,
  });
};

/**
 * Return true if the census is up to date with regards to the current filtering
 * and requested display, false otherwise.
 *
 * @param {String} filter
 * @param {censusDisplayModel} display
 * @param {censusModel} census
 *
 * @returns {Boolean}
 */
exports.censusIsUpToDate = function (filter, display, census) {
  return census
      // Filter could be null == undefined so use loose equality.
      && filter == census.filter
      && display === census.display;
};


/**
 * Check to see if the snapshot is in a state that it can take a census.
 *
 * @param {SnapshotModel} A snapshot to check.
 * @param {Boolean} Assert that the snapshot must be in a ready state.
 * @returns {Boolean}
 */
exports.canTakeCensus = function (snapshot) {
  return snapshot.state === states.READ &&
    ((!snapshot.census || snapshot.census.state === censusState.SAVED) ||
     (!snapshot.treeMap || snapshot.treeMap.state === treeMapState.SAVED));
};

/**
 * Returns true if the given snapshot's dominator tree has been computed, false
 * otherwise.
 *
 * @param {SnapshotModel} snapshot
 * @returns {Boolean}
 */
exports.dominatorTreeIsComputed = function (snapshot) {
  return snapshot.dominatorTree &&
    (snapshot.dominatorTree.state === dominatorTreeState.COMPUTED ||
     snapshot.dominatorTree.state === dominatorTreeState.LOADED ||
     snapshot.dominatorTree.state === dominatorTreeState.INCREMENTAL_FETCHING);
};

/**
 * Find the first SAVED census, either from the tree map or the normal
 * census.
 *
 * @param {SnapshotModel} snapshot
 * @returns {Object|null} Either the census, or null if one hasn't completed
 */
exports.getSavedCensus = function (snapshot) {
  if (snapshot.treeMap && snapshot.treeMap.state === treeMapState.SAVED) {
    return snapshot.treeMap;
  }
  if (snapshot.census && snapshot.census.state === censusState.SAVED) {
    return snapshot.census;
  }
  return null;
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
exports.openFilePicker = function ({ title, filters, defaultName, mode }) {
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

/**
 * Format the provided number with a space every 3 digits, and optionally
 * prefixed by its sign.
 *
 * @param {Number} number
 * @param {Boolean} showSign (defaults to false)
 */
exports.formatNumber = function (number, showSign = false) {
  const rounded = Math.round(number);
  if (rounded === 0 || rounded === -0) {
    return "0";
  }

  const abs = String(Math.abs(rounded));
  // replace every digit followed by (sets of 3 digits) by (itself and a space)
  const formatted = abs.replace(/(\d)(?=(\d{3})+$)/g, "$1 ");

  if (showSign) {
    const sign = rounded < 0 ? "-" : "+";
    return sign + formatted;
  }
  return formatted;
};

/**
 * Format the provided percentage following the same logic as formatNumber and
 * an additional % suffix.
 *
 * @param {Number} percent
 * @param {Boolean} showSign (defaults to false)
 */
exports.formatPercent = function (percent, showSign = false) {
  return exports.L10N.getFormatStr("tree-item.percent2",
                           exports.formatNumber(percent, showSign));
};

/**
 * Change an HSL color array with values ranged 0-1 to a properly formatted
 * ctx.fillStyle string.
 *
 * @param  {Number} h
 *         hue values ranged between [0 - 1]
 * @param  {Number} s
 *         hue values ranged between [0 - 1]
 * @param  {Number} l
 *         hue values ranged between [0 - 1]
 * @return {type}
 */
exports.hslToStyle = function (h, s, l) {
  h = parseInt(h * 360, 10);
  s = parseInt(s * 100, 10);
  l = parseInt(l * 100, 10);

  return `hsl(${h},${s}%,${l}%)`;
};

/**
 * Linearly interpolate between 2 numbers.
 *
 * @param {Number} a
 * @param {Number} b
 * @param {Number} t
 *        A value of 0 returns a, and 1 returns b
 * @return {Number}
 */
exports.lerp = function (a, b, t) {
  return a * (1 - t) + b * t;
};

/**
 * Format a number of bytes as human readable, e.g. 13434 => '13KiB'.
 *
 * @param  {Number} n
 *         Number of bytes
 * @return {String}
 */
exports.formatAbbreviatedBytes = function (n) {
  if (n < BYTES) {
    return n + "B";
  } else if (n < KILOBYTES) {
    return Math.floor(n / BYTES) + "KiB";
  } else if (n < MEGABYTES) {
    return Math.floor(n / KILOBYTES) + "MiB";
  }
  return Math.floor(n / MEGABYTES) + "GiB";
};
