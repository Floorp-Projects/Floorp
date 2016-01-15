/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Load the shared test helpers into this compartment.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/framework/test/shared-head.js",
  this);

var { snapshotState: states } = require("devtools/client/memory/constants");
var { breakdownEquals, breakdownNameToSpec } = require("devtools/client/memory/utils");

Services.prefs.setBoolPref("devtools.memory.enabled", true);

/**
 * Open the memory panel for the given tab.
 */
this.openMemoryPanel = Task.async(function* (tab) {
  info("Opening memory panel.");
  const target = TargetFactory.forTab(tab);
  const toolbox = yield gDevTools.showToolbox(target, "memory");
  info("Memory panel shown successfully.");
  let panel = toolbox.getCurrentPanel();
  return { tab, panel };
});

/**
 * Close the memory panel for the given tab.
 */
this.closeMemoryPanel = Task.async(function* (tab) {
  info("Closing memory panel.");
  const target = TargetFactory.forTab(tab);
  const toolbox = gDevTools.getToolbox(target);
  yield toolbox.destroy();
  info("Closed memory panel successfully.");
});

/**
 * Return a test function that adds a tab with the given url, opens the memory
 * panel, runs the given generator, closes the memory panel, removes the tab,
 * and finishes.
 *
 * Example usage:
 *
 *     this.test = makeMemoryTest(TEST_URL, function* ({ tab, panel }) {
 *         // Your tests go here...
 *     });
 */
function makeMemoryTest(url, generator) {
  return Task.async(function* () {
    waitForExplicitFinish();

    const tab = yield addTab(url);
    const results = yield openMemoryPanel(tab);

    try {
      yield* generator(results);
    } catch (err) {
      ok(false, "Got an error: " + DevToolsUtils.safeErrorString(err));
    }

    yield closeMemoryPanel(tab);
    yield removeTab(tab);

    finish();
  });
}


function waitUntilState (store, predicate) {
  let deferred = promise.defer();
  let unsubscribe = store.subscribe(check);

  function check () {
    if (predicate(store.getState())) {
      unsubscribe();
      deferred.resolve()
    }
  }

  // Fire the check immediately incase the action has already occurred
  check();

  return deferred.promise;
}

function waitUntilSnapshotState (store, expected) {
  let predicate = () => {
    let snapshots = store.getState().snapshots;
    info(snapshots.map(x => x.state));
    return snapshots.length === expected.length &&
           expected.every((state, i) => state === "*" || snapshots[i].state === state);
  };
  info(`Waiting for snapshots to be of state: ${expected}`);
  return waitUntilState(store, predicate);
}

function takeSnapshot (window) {
  let { gStore, document } = window;
  let snapshotCount = gStore.getState().snapshots.length;
  info(`Taking snapshot...`);
  document.querySelector(".devtools-toolbar .take-snapshot").click();
  return waitUntilState(gStore, () => gStore.getState().snapshots.length === snapshotCount + 1);
}

/**
 * Sets breakdown and waits for currently selected breakdown to use it
 * and be completed the census.
 */
function setBreakdown (window, type) {
  info(`Setting breakdown to ${type}...`);
  let { gStore, gHeapAnalysesClient } = window;
  // XXX: Should handle this via clicking the DOM, but React doesn't
  // fire the onChange event, so just change it in the store.
  // window.document.querySelector(`.select-breakdown`).value = type;
  gStore.dispatch(require("devtools/client/memory/actions/breakdown")
                         .setBreakdownAndRefresh(gHeapAnalysesClient, breakdownNameToSpec(type)));

  return waitUntilState(window.gStore, () => {
    let selected = window.gStore.getState().snapshots.find(s => s.selected);
    return selected.state === states.SAVED_CENSUS &&
           breakdownEquals(breakdownNameToSpec(type), selected.census.breakdown);
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
