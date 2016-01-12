/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests taking and then clearing snapshots.
 */

const TEST_URL = "http://example.com/browser/devtools/client/memory/test/browser/doc_steady_allocation.html";

this.test = makeMemoryTest(TEST_URL, function* ({ tab, panel }) {
  const { gStore, document } = panel.panelWin;
  const { getState, dispatch } = gStore;

  let snapshotEls = document.querySelectorAll("#memory-tool-container .list li");
  is(getState().snapshots.length, 0, "Starts with no snapshots in store");
  is(snapshotEls.length, 0, "No snapshots visible");

  info("Take two snapshots");
  yield takeSnapshot(panel.panelWin);
  yield takeSnapshot(panel.panelWin);
  yield waitUntilSnapshotState(gStore, [states.SAVED_CENSUS, states.SAVED_CENSUS]);

  snapshotEls = document.querySelectorAll("#memory-tool-container .list li");
  is(snapshotEls.length, 2, "Two snapshots visible");

  info("Click on Clear Snapshots");
  yield clearSnapshots(panel.panelWin);
  is(getState().snapshots.length, 0, "No snapshots in store");
  snapshotEls = document.querySelectorAll("#memory-tool-container .list li");
  is(snapshotEls.length, 0, "No snapshot visible");
});
