/* -*- indent-tabs-mode: nil; js-indent-level: 2; fill-column: 80 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function run_test() {
  var sanityTest = new SanityTest();
  var results = run_beautifier_tests(sanityTest,
                                     Urlencoded,
                                     beautify.js,
                                     beautify.html,
                                     beautify.css);

  for (let [test_name, parameters, expected_value, result] of sanityTest.successes) {
    equal(result, expected_value, "actual result matches expected");
  }

  for (let [test_name, parameters, expected_value, result] of sanityTest.failures) {
    equal(result, expected_value, "actual result matches expected");
  }
}
