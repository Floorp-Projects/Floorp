/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* global ChromeUtils */

// Test that refreshing the page with devtools open does not leak the old
// windows from previous navigations.
//
// IF THIS TEST STARTS FAILING, YOU ARE LEAKING EVERY WINDOW EVER NAVIGATED TO
// WHILE DEVTOOLS ARE OPEN! THIS IS NOT SPECIFIC TO THE MEMORY TOOL ONLY!

"use strict";

const { getLabelAndShallowSize } = require("devtools/shared/heapsnapshot/DominatorTreeNode");

const TEST_URL = "http://example.com/browser/devtools/client/memory/test/browser/doc_empty.html";

async function getWindowsInSnapshot(front) {
  dumpn("Taking snapshot.");
  const path = await front.saveHeapSnapshot();
  dumpn("Took snapshot with path = " + path);
  const snapshot = ChromeUtils.readHeapSnapshot(path);
  dumpn("Read snapshot into memory, taking census.");
  const report = snapshot.takeCensus({
    breakdown: {
      by: "objectClass",
      then: { by: "bucket" },
      other: { by: "count", count: true, bytes: false },
    }
  });
  dumpn("Took census, window count = " + report.Window.count);
  return report.Window;
}

const DESCRIPTION = {
  by: "coarseType",
  objects: {
    by: "objectClass",
    then: { by: "count", count: true, bytes: false },
    other: { by: "count", count: true, bytes: false },
  },
  strings: { by: "count", count: true, bytes: false },
  scripts: {
    by: "internalType",
    then: { by: "count", count: true, bytes: false },
  },
  other: {
    by: "internalType",
    then: { by: "count", count: true, bytes: false },
  }
};

this.test = makeMemoryTest(TEST_URL, async function({ tab, panel }) {
  const front = panel.panelWin.gFront;

  const startWindows = await getWindowsInSnapshot(front);
  dumpn("Initial windows found = " + startWindows.map(w => "0x" +
                                     w.toString(16)).join(", "));
  is(startWindows.length, 1);

  await refreshTab();

  const endWindows = await getWindowsInSnapshot(front);
  is(endWindows.length, 1);

  if (endWindows.length === 1) {
    return;
  }

  dumpn("Test failed, diagnosing leaking windows.");
  dumpn("(This may fail if a moving GC has relocated the initial Window objects.)");

  dumpn("Taking full runtime snapshot.");
  const path = await front.saveHeapSnapshot({ boundaries: { runtime: true } });
  dumpn("Full runtime's snapshot path = " + path);

  dumpn("Reading full runtime heap snapshot.");
  const snapshot = ChromeUtils.readHeapSnapshot(path);
  dumpn("Done reading full runtime heap snapshot.");

  const dominatorTree = snapshot.computeDominatorTree();
  const paths = snapshot.computeShortestPaths(dominatorTree.root, startWindows, 50);

  for (let i = 0; i < startWindows.length; i++) {
    dumpn("Shortest retaining paths for leaking Window 0x" +
          startWindows[i].toString(16) + " =========================");
    let j = 0;
    for (const retainingPath of paths.get(startWindows[i])) {
      if (retainingPath.find(part => part.predecessor === startWindows[i])) {
        // Skip paths that loop out from the target window and back to it again.
        continue;
      }

      dumpn("    Path #" + (++j) +
            ": --------------------------------------------------------------------");
      for (const part of retainingPath) {
        const { label } = getLabelAndShallowSize(part.predecessor, snapshot, DESCRIPTION);
        dumpn("        0x" + part.predecessor.toString(16) +
              " (" + label.join(" > ") + ")");
        dumpn("               |");
        dumpn("              " + part.edge);
        dumpn("               |");
        dumpn("               V");
      }
      dumpn("        0x" + startWindows[i].toString(16) + " (objects > Window)");
    }
  }
});
