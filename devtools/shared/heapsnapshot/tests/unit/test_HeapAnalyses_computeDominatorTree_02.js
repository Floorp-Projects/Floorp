/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test the HeapAnalyses{Client,Worker} "computeDominatorTree" request with bad
// file paths.

add_task(async function () {
  const client = new HeapAnalysesClient();

  let threw = false;
  try {
    await client.computeDominatorTree("/etc/passwd");
  } catch (_) {
    threw = true;
  }
  ok(threw, "should throw when given a bad path");

  client.destroy();
});
