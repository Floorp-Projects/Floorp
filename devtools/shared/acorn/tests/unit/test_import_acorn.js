/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we can require acorn.
 */

function run_test() {
  const acorn = require("acorn/acorn");
  const acorn_loose = require("acorn/acorn_loose");
  const walk = require("acorn/util/walk");
  do_check_true(isObject(acorn));
  do_check_true(isObject(acorn_loose));
  do_check_true(isObject(walk));
  do_check_eq(typeof acorn.parse, "function");
  do_check_eq(typeof acorn_loose.parse_dammit, "function");
  do_check_eq(typeof walk.simple, "function");
}
