/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Load the shared test helpers into this compartment.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/shared-head.js",
  this
);

var {
  censusDisplays,
  censusState,
  snapshotState: states,
} = require("resource://devtools/client/memory/constants.js");
var { L10N } = require("resource://devtools/client/memory/utils.js");

Services.prefs.setBoolPref("devtools.memory.enabled", true);

/**
 * Open the memory panel for the given tab.
 */
this.openMemoryPanel = async function (tab) {
  info("Opening memory panel.");
  const toolbox = await gDevTools.showToolboxForTab(tab, { toolId: "memory" });
  info("Memory panel shown successfully.");
  const panel = toolbox.getCurrentPanel();
  return { tab, panel };
};

/**
 * Close the memory panel for the given tab.
 */
this.closeMemoryPanel = async function (tab) {
  info("Closing memory panel.");
  const toolbox = gDevTools.getToolboxForTab(tab);
  await toolbox.destroy();
  info("Closed memory panel successfully.");
};

/**
 * Return a test function that adds a tab with the given url, opens the memory
 * panel, runs the given generator, closes the memory panel, removes the tab,
 * and finishes.
 *
 * Example usage:
 *
 *     this.test = makeMemoryTest(TEST_URL, async function ({ tab, panel }) {
 *         // Your tests go here...
 *     });
 */
function makeMemoryTest(url, generator) {
  return async function () {
    waitForExplicitFinish();

    // It can take a long time to save a snapshot to disk, read the snapshots
    // back from disk, and finally perform analyses on them.
    requestLongerTimeout(2);

    const tab = await addTab(url);
    const results = await openMemoryPanel(tab);

    try {
      await generator(results);
    } catch (err) {
      ok(false, "Got an error: " + DevToolsUtils.safeErrorString(err));
    }

    await closeMemoryPanel(tab);
    await removeTab(tab);

    finish();
  };
}

function dumpn(msg) {
  dump(`MEMORY-TEST: ${msg}\n`);
}

/**
 * Returns a promise that will resolve when the provided store matches
 * the expected array. expectedStates is an array of dominatorTree states.
 * Expectations :
 * - store.getState().snapshots.length == expected.length
 * - snapshots[i].dominatorTree.state == expected[i]
 *
 * @param  {Store} store
 * @param  {Array<string>} expectedStates [description]
 * @return {Promise}
 */
function waitUntilDominatorTreeState(store, expected) {
  const predicate = () => {
    const snapshots = store.getState().snapshots;
    return (
      snapshots.length === expected.length &&
      expected.every((state, i) => {
        return (
          snapshots[i].dominatorTree &&
          snapshots[i].dominatorTree.state === state
        );
      })
    );
  };
  info(`Waiting for dominator trees to be of state: ${expected}`);
  return waitUntilState(store, predicate);
}

function takeSnapshot(window) {
  const { gStore, document } = window;
  const snapshotCount = gStore.getState().snapshots.length;
  info("Taking snapshot...");
  document.querySelector(".devtools-toolbar .take-snapshot").click();
  return waitUntilState(
    gStore,
    () => gStore.getState().snapshots.length === snapshotCount + 1
  );
}

function clearSnapshots(window) {
  const { gStore, document } = window;
  document.querySelector(".devtools-toolbar .clear-snapshots").click();
  return waitUntilState(gStore, () =>
    gStore
      .getState()
      .snapshots.every(snapshot => snapshot.state !== states.READ)
  );
}

/**
 * Sets the current requested display and waits for the selected snapshot to use
 * it and complete the new census that entails.
 */
function setCensusDisplay(window, display) {
  info(`Setting census display to ${display}...`);
  const { gStore, gHeapAnalysesClient } = window;
  // XXX: Should handle this via clicking the DOM, but React doesn't
  // fire the onChange event, so just change it in the store.
  // window.document.querySelector(`.select-display`).value = type;
  gStore.dispatch(
    require("resource://devtools/client/memory/actions/census-display.js").setCensusDisplayAndRefresh(
      gHeapAnalysesClient,
      display
    )
  );

  return waitUntilState(window.gStore, () => {
    const selected = window.gStore.getState().snapshots.find(s => s.selected);
    return (
      selected.state === states.READ &&
      selected.census &&
      selected.census.state === censusState.SAVED &&
      selected.census.display === display
    );
  });
}

/**
 * Get the snapshot tatus text currently displayed, or null if none is
 * displayed.
 *
 * @param {Document} document
 */
function getDisplayedSnapshotStatus(document) {
  const status = document.querySelector(".snapshot-status");
  return status ? status.textContent.trim() : null;
}

/**
 * Get the index of the currently selected snapshot.
 *
 * @return {Number}
 */
function getSelectedSnapshotIndex(store) {
  const snapshots = store.getState().snapshots;
  const selectedSnapshot = snapshots.find(s => s.selected);
  return snapshots.indexOf(selectedSnapshot);
}

/**
 * Returns a promise that will resolve when the snapshot with provided index
 * becomes selected.
 *
 * @return {Promise}
 */
function waitUntilSnapshotSelected(store, snapshotIndex) {
  return waitUntilState(
    store,
    state =>
      state.snapshots[snapshotIndex] &&
      state.snapshots[snapshotIndex].selected === true
  );
}

/**
 * Wait until the state has censuses in a certain state.
 *
 * @return {Promise}
 */
function waitUntilCensusState(store, getCensus, expected) {
  const predicate = () => {
    const snapshots = store.getState().snapshots;

    info(
      "Current census state:" +
        snapshots.map(x => (getCensus(x) ? getCensus(x).state : null))
    );

    return (
      snapshots.length === expected.length &&
      expected.every((state, i) => {
        const census = getCensus(snapshots[i]);
        return (
          state === "*" ||
          (!census && !state) ||
          (census && census.state === state)
        );
      })
    );
  };
  info(`Waiting for snapshot censuses to be of state: ${expected}`);
  return waitUntilState(store, predicate);
}

/**
 * Mock out the requestAnimationFrame.
 *
 * @return {Object}
 * @function nextFrame
 *           Call the last queued function
 * @function raf
 *           The mocked raf function
 * @function timesCalled
 *           How many times the RAF has been called
 */
function createRAFMock() {
  let queuedFns = [];
  const mock = { timesCalled: 0 };

  mock.nextFrame = function () {
    const thisQueue = queuedFns;
    queuedFns = [];
    for (let i = 0; i < thisQueue.length; i++) {
      thisQueue[i]();
    }
  };

  mock.raf = function (fn) {
    mock.timesCalled++;
    queuedFns.push(fn);
  };
  return mock;
}

/**
 * Test to see if two floats are equivalent.
 *
 * @param {Float} a
 * @param {Float} b
 * @return {Boolean}
 */
function floatEquality(a, b) {
  const EPSILON = 0.00000000001;
  const equals = Math.abs(a - b) < EPSILON;
  if (!equals) {
    info(`${a} not equal to ${b}`);
  }
  return equals;
}
