/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test deleteHeapSnapshot is a noop if the provided path matches no snapshot

function run_test() {
  run_next_test();
}

add_task(function* () {
  const client = new HeapAnalysesClient();

  let threw = false;
  try {
    yield client.deleteHeapSnapshot("path-does-not-exist");
  } catch (_) {
    threw = true;
  }
  ok(threw, "deleteHeapSnapshot on non-existant path should throw an error");

  client.destroy();
});
