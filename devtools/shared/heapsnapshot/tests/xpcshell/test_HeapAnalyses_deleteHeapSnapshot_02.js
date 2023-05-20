/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test deleteHeapSnapshot is a noop if the provided path matches no snapshot

add_task(async function () {
  const client = new HeapAnalysesClient();

  let threw = false;
  try {
    await client.deleteHeapSnapshot("path-does-not-exist");
  } catch (_) {
    threw = true;
  }
  ok(threw, "deleteHeapSnapshot on non-existant path should throw an error");

  client.destroy();
});
