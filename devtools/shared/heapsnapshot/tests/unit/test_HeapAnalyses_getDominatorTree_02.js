/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test the HeapAnalyses{Client,Worker} "getDominatorTree" request with bad
// dominator tree ids.

const breakdown = {
  by: "coarseType",
  objects: { by: "count", count: true, bytes: true },
  scripts: { by: "count", count: true, bytes: true },
  strings: { by: "count", count: true, bytes: true },
  other: { by: "count", count: true, bytes: true },
};

add_task(async function () {
  const client = new HeapAnalysesClient();

  let threw = false;
  try {
    await client.getDominatorTree({ dominatorTreeId: 42, breakdown });
  } catch (_) {
    threw = true;
  }
  ok(threw, "should throw when given a bad id");

  client.destroy();
});
