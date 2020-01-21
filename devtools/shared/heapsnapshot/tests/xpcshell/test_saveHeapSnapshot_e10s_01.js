/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test saving a heap snapshot in the sandboxed e10s child process.

function run_test() {
  run_test_in_child("test_SaveHeapSnapshot.js");
}
