/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the devtools/shared/worker can handle:
// returned primitives (or promise or Error)
//
// And tests `workerify` by doing so.

const { workerify } = require("resource://devtools/shared/worker/worker.js");
function square(x) {
  return x * x;
}

function squarePromise(x) {
  return new Promise(resolve => resolve(x * x));
}

function squareError(x) {
  return new Error("Nope");
}

function squarePromiseReject(x) {
  return new Promise((_, reject) => reject("Nope"));
}

registerCleanupFunction(function () {
  Services.prefs.clearUserPref("security.allow_parent_unrestricted_js_loads");
});

add_task(async function () {
  // Needed for blob:null
  Services.prefs.setBoolPref(
    "security.allow_parent_unrestricted_js_loads",
    true
  );
  let fn = workerify(square);
  is(await fn(5), 25, "return primitives successful");
  fn.destroy();

  fn = workerify(squarePromise);
  is(await fn(5), 25, "promise primitives successful");
  fn.destroy();

  fn = workerify(squareError);
  try {
    await fn(5);
    ok(false, "return error should reject");
  } catch (e) {
    ok(true, "return error should reject");
  }
  fn.destroy();

  fn = workerify(squarePromiseReject);
  try {
    await fn(5);
    ok(false, "returned rejected promise rejects");
  } catch (e) {
    ok(true, "returned rejected promise rejects");
  }
  fn.destroy();
});
