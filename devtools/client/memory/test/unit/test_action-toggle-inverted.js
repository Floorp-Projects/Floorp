/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test toggling the top level inversion state of the tree.

let { toggleInverted } = require("devtools/client/memory/actions/inverted");

function run_test() {
  run_next_test();
}

add_task(function *() {
  let front = new StubbedMemoryFront();
  let heapWorker = new HeapAnalysesClient();
  yield front.attach();
  let store = Store();
  const { getState, dispatch } = store;

  equal(getState().inverted, false, "not inverted by default");

  dispatch(toggleInverted());
  equal(getState().inverted, true, "now inverted after toggling");

  dispatch(toggleInverted());
  equal(getState().inverted, false, "not inverted again after toggling again");

  heapWorker.destroy();
  yield front.detach();
});
