/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test toggling of diffing.

const {
  toggleDiffing,
} = require("resource://devtools/client/memory/actions/diffing.js");

add_task(async function () {
  const front = new StubbedMemoryFront();
  const heapWorker = new HeapAnalysesClient();
  await front.attach();
  const store = Store();
  const { getState, dispatch } = store;

  equal(getState().diffing, null, "not diffing by default");

  dispatch(toggleDiffing());
  ok(getState().diffing, "now diffing after toggling");

  dispatch(toggleDiffing());
  equal(getState().diffing, null, "not diffing again after toggling again");

  heapWorker.destroy();
  await front.detach();
});
