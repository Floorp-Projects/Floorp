/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the devtools/shared/worker can handle:
// returned primitives (or promise or Error)
//
// And tests `workerify` by doing so.

const { DevToolsWorker, workerify } = require("devtools/shared/worker/worker");
function square(x) {
  return x * x;
}

function squarePromise(x) {
  return new Promise((resolve) => resolve(x * x));
}

function squareError(x) {
  return new Error("Nope");
}

function squarePromiseReject(x) {
  return new Promise((_, reject) => reject("Nope"));
}

add_task(function* () {
  let fn = workerify(square);
  is((yield fn(5)), 25, "return primitives successful");
  fn.destroy();

  fn = workerify(squarePromise);
  is((yield fn(5)), 25, "promise primitives successful");
  fn.destroy();

  fn = workerify(squareError);
  try {
    yield fn(5);
    ok(false, "return error should reject");
  } catch (e) {
    ok(true, "return error should reject");
  }
  fn.destroy();

  fn = workerify(squarePromiseReject);
  try {
    yield fn(5);
    ok(false, "returned rejected promise rejects");
  } catch (e) {
    ok(true, "returned rejected promise rejects");
  }
  fn.destroy();
});
