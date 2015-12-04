/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the HeapAnalyses{Client,Worker} "computeDominatorTree" request with bad
// file paths.

function run_test() {
  run_next_test();
}

add_task(function* () {
  const client = new HeapAnalysesClient();

  let threw = false;
  try {
    yield client.computeDominatorTree("/etc/passwd");
  } catch (_) {
    threw = true;
  }
  ok(threw, "should throw when given a bad path");

  client.destroy();
});
