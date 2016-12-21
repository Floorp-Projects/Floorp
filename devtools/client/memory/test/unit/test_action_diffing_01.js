/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test toggling of diffing.

const { toggleDiffing } = require("devtools/client/memory/actions/diffing");

function run_test() {
  run_next_test();
}

add_task(function* () {
  let front = new StubbedMemoryFront();
  let heapWorker = new HeapAnalysesClient();
  yield front.attach();
  let store = Store();
  const { getState, dispatch } = store;

  equal(getState().diffing, null, "not diffing by default");

  dispatch(toggleDiffing());
  ok(getState().diffing, "now diffing after toggling");

  dispatch(toggleDiffing());
  equal(getState().diffing, null, "not diffing again after toggling again");

  heapWorker.destroy();
  yield front.detach();
});
