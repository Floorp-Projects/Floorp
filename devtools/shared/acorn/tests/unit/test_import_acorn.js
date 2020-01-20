/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we can require acorn.
 */

function run_test() {
  const acorn = require("acorn/acorn");
  const acorn_loose = require("acorn/acorn_loose");
  const walk = require("acorn/util/walk");
  Assert.ok(isObject(acorn));
  Assert.ok(isObject(acorn_loose));
  Assert.ok(isObject(walk));
  Assert.equal(typeof acorn.parse, "function");
  Assert.equal(typeof acorn_loose.parse_dammit, "function");
  Assert.equal(typeof walk.simple, "function");
}
