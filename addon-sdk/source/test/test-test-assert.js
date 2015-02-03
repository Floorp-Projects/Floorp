/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Assert } = require("sdk/test/assert");

function createAssertTest() {
  let failures = [], successes = [], exceptions = [];
  return {
    test: new Assert({
      fail: (m) => failures.push(m),
      pass: (m) => successes.push(m),
      exception: (e) => exceptions.push(e)
    }),
    failures: failures,
    successes: successes,
    exceptions: exceptions
  };
}

exports["test createAssertTest initial state"] = function(assert) {
  let { failures, successes, exceptions } = createAssertTest();

  assert.equal(successes.length, 0, "0 success log");
  assert.equal(failures.length, 0, "0 failure logs");
  assert.equal(exceptions.length, 0, "0 exception logs");
}

exports["test assert.ok(true)"] = (assert) => {
  let { test, failures, successes, exceptions } = createAssertTest();

  assert.strictEqual(test.ok(true), true, "assert ok(true) strictEquals true");
  assert.equal(successes.length, 1, "1 success log");

  assert.equal(test.ok(true), true, "assert ok(true) equals true");
  assert.equal(successes.length, 2, "2 success logs");

  assert.ok(test.ok(true), "assert ok(true) is ok");
  assert.equal(successes.length, 3, "3 success logs");

  assert.equal(failures.length, 0, "0 failure logs");
  assert.equal(exceptions.length, 0, "0 exception logs");
}

exports["test assert.ok(false)"] = (assert) => {
  let { test, failures, successes, exceptions } = createAssertTest();

  assert.strictEqual(test.ok(false), false, "assert ok(false) strictEquals false");
  assert.equal(failures.length, 1, "1 failure log");

  assert.equal(test.ok(false), false, "assert ok(false) equals false");
  assert.equal(failures.length, 2, "2 failure logs");

  assert.ok(!test.ok(false), "assert ok(false) is not ok");
  assert.equal(failures.length, 3, "3 failure logs");

  assert.equal(successes.length, 0, "0 success log");
  assert.equal(exceptions.length, 0, "0 exception logs");
}

exports["test assert.ok(false) failure message"] = (assert) => {
  let { test, failures, successes, exceptions } = createAssertTest();

  test.ok(false, "XYZ");

  assert.equal(successes.length, 0, "0 success log");
  assert.equal(failures.length, 1, "1 failure logs");
  assert.equal(exceptions.length, 0, "0 exception logs");

  assert.equal(failures[0], "XYZ - false == true");
}

exports["test assert.equal"] = (assert) => {
  let { test, failures, successes, exceptions } = createAssertTest();

  assert.strictEqual(test.equal(true, true), true, "assert equal(true, true) strictEquals true");
  assert.equal(test.equal(true, true), true, "assert equal(true, true) equals true");
  assert.ok(test.equal(true, true), "assert equal(true, true) is ok");

  assert.equal(successes.length, 3, "3 success log");
  assert.equal(failures.length, 0, "0 failure logs");
  assert.equal(exceptions.length, 0, "0 exception logs");
}

exports["test assert.equal failure message"] = (assert) => {
  let { test, failures, successes, exceptions } = createAssertTest();

  test.equal("foo", "bar", "XYZ");

  assert.equal(successes.length, 0, "0 success log");
  assert.equal(failures.length, 1, "1 failure logs");
  assert.equal(exceptions.length, 0, "0 exception logs");

  assert.equal(failures[0], "XYZ - \"foo\" == \"bar\"");
}

exports["test assert.strictEqual"] = (assert) => {
  let { test, failures, successes, exceptions } = createAssertTest();

  assert.strictEqual(test.strictEqual(true, true), true, "assert strictEqual(true, true) strictEquals true");
  assert.equal(successes.length, 1, "1 success logs");

  assert.equal(test.strictEqual(true, true), true, "assert strictEqual(true, true) equals true");
  assert.equal(successes.length, 2, "2 success logs");

  assert.ok(test.strictEqual(true, true), "assert strictEqual(true, true) is ok");
  assert.equal(successes.length, 3, "3 success logs");

  assert.equal(failures.length, 0, "0 failure logs");
  assert.equal(exceptions.length, 0, "0 exception logs");
}

exports["test assert.strictEqual failure message"] = (assert) => {
  let { test, failures, successes, exceptions } = createAssertTest();

  test.strictEqual("foo", "bar", "XYZ");

  assert.equal(successes.length, 0, "0 success log");
  assert.equal(failures.length, 1, "1 failure logs");
  assert.equal(exceptions.length, 0, "0 exception logs");

  assert.equal(failures[0], "XYZ - \"foo\" === \"bar\"");
}

exports["test assert.throws(func, string, string) matches"] = (assert) => {
  let { test, failures, successes, exceptions } = createAssertTest();

  assert.ok(
    test.throws(
      () => { throw new Error("this is a thrown error") },
      "this is a thrown error"
    ),
    "throwing an new Error works");
  assert.equal(successes.length, 1, "1 success log");

  assert.ok(
    test.throws(
      () => { throw Error("this is a thrown error") },
      "this is a thrown error"
    ),
    "throwing an Error works");
  assert.equal(successes.length, 2, "2 success log");

  assert.ok(
    test.throws(
      () => { throw "this is a thrown string" },
      "this is a thrown string"
    ),
    "throwing a String works");
  assert.equal(successes.length, 3, "3 success logs");

  assert.equal(failures.length, 0, "0 failure logs");
  assert.equal(exceptions.length, 0, "0 exception logs");
}

exports["test assert.throws(func, string, string) failure message"] = (assert) => {
  let { test, failures, successes, exceptions } = createAssertTest();

  test.throws(
    () => { throw new Error("foo") },
    "bar",
    "XYZ");

  assert.equal(successes.length, 0, "0 success log");
  assert.equal(failures.length, 1, "1 failure logs");
  assert.equal(exceptions.length, 0, "0 exception logs");

  assert.equal(failures[0], "XYZ - \"foo\" matches \"bar\"");
}

exports["test assert.throws(func, regex, string) matches"] = (assert) => {
  let { test, failures, successes, exceptions } = createAssertTest();

  assert.ok(
    test.throws(
      () => { throw new Error("this is a thrown error") },
      /this is a thrown error/
    ),
    "throwing an new Error works");
  assert.equal(successes.length, 1, "1 success log");

  assert.ok(
    test.throws(
      () => { throw Error("this is a thrown error") },
      /this is a thrown error/
    ),
    "throwing an Error works");
  assert.equal(successes.length, 2, "2 success log");

  assert.ok(
    test.throws(
      () => { throw "this is a thrown string" },
      /this is a thrown string/
    ),
    "throwing a String works");
  assert.equal(successes.length, 3, "3 success logs");

  assert.equal(failures.length, 0, "0 failure logs");
  assert.equal(exceptions.length, 0, "0 exception logs");
}

exports["test assert.throws(func, regex, string) failure message"] = (assert) => {
  let { test, failures, successes, exceptions } = createAssertTest();

  test.throws(
    () => { throw new Error("foo") },
    /bar/i,
    "XYZ");

  assert.equal(successes.length, 0, "0 success log");
  assert.equal(failures.length, 1, "1 failure logs");
  assert.equal(exceptions.length, 0, "0 exception logs");

  assert.equal(failures[0], "XYZ - \"foo\" matches \"/bar/i\"");
}

require("sdk/test").run(exports);
