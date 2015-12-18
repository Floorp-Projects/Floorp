/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the HeapAnalyses{Client,Worker} "getDominatorTree" request with bad
// dominator tree ids.

function run_test() {
  run_next_test();
}

const breakdown = {
  by: "coarseType",
  objects: { by: "count", count: true, bytes: true },
  scripts: { by: "count", count: true, bytes: true },
  strings: { by: "count", count: true, bytes: true },
  other: { by: "count", count: true, bytes: true },
};

add_task(function* () {
  const client = new HeapAnalysesClient();

  let threw = false;
  try {
    yield client.getDominatorTree({ dominatorTreeId: 42, breakdown });
  } catch (_) {
    threw = true;
  }
  ok(threw, "should throw when given a bad id");

  client.destroy();
});
