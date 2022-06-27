/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Sanity test that we can compute dominator trees.

function run_test() {
  const path = ChromeUtils.saveHeapSnapshot({ runtime: true });
  const snapshot = ChromeUtils.readHeapSnapshot(path);

  const dominatorTree = snapshot.computeDominatorTree();
  ok(dominatorTree);
  ok(DominatorTree.isInstance(dominatorTree));

  let threw = false;
  try {
    new DominatorTree();
  } catch (e) {
    threw = true;
  }
  ok(threw, "Constructor shouldn't be usable");

  do_test_finished();
}
