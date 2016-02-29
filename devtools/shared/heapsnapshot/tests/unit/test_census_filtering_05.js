/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that filtered and inverted allocation stack census trees are sorted
// properly.

function run_test() {
  const countBreakdown = { by: "count", count: true, bytes: true };

  const BREAKDOWN = {
    by: "allocationStack",
    then: countBreakdown,
    noStack: countBreakdown,
  };

  const stacks = [];

  function foo(depth = 1) {
    stacks.push(saveStack(depth));
    bar(depth + 1);
    baz(depth + 1);
    stacks.push(saveStack(depth));
  }

  function bar(depth = 1) {
    stacks.push(saveStack(depth));
    stacks.push(saveStack(depth));
  }

  function baz(depth = 1) {
    stacks.push(saveStack(depth));
    bang(depth + 1);
    stacks.push(saveStack(depth));
  }

  function bang(depth = 1) {
    stacks.push(saveStack(depth));
    stacks.push(saveStack(depth));
    stacks.push(saveStack(depth));
  }

  foo();
  bar();
  baz();
  bang();

  const REPORT = new Map(stacks.map((s, i) => {
    return [s, {
      count: i + 1,
      bytes: (i + 1) * 10
    }];
  }));

  const tree = censusReportToCensusTreeNode(BREAKDOWN, REPORT, {
    filter: "baz",
    invert: true
  });

  dumpn("tree = " + JSON.stringify(tree, savedFrameReplacer, 4));

  (function assertSortedBySelf(node) {
    if (node.children) {
      let lastSelfBytes = Infinity;
      for (let child of node.children) {
        ok(child.bytes <= lastSelfBytes, `${child.bytes} <= ${lastSelfBytes}`);
        lastSelfBytes = child.bytes;
        assertSortedBySelf(child);
      }
    }
  }(tree));
}
