/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/

"use strict";
const { require } = Components.utils.import("resource://devtools/shared/Loader.jsm", {});
const { FallibleJSPropertyProvider: JSPropertyProvider } =
  require("devtools/shared/webconsole/js-property-provider");

Components.utils.import("resource://gre/modules/jsdebugger.jsm");
addDebuggerToGlobal(this);

function run_test() {
  const testArray = `var testArray = [
    {propA: "A"},
    {
      propB: "B",
      propC: [
        "D"
      ]
    },
    [
      {propE: "E"}
    ]
  ]`;

  const testObject = 'var testObject = {"propA": [{"propB": "B"}]}';
  const testHyphenated = 'var testHyphenated = {"prop-A": "res-A"}';
  const testLet = "let foobar = {a: ''}; const blargh = {a: 1};";

  let sandbox = Components.utils.Sandbox("http://example.com");
  let dbg = new Debugger;
  let dbgObject = dbg.addDebuggee(sandbox);
  let dbgEnv = dbgObject.asEnvironment();
  Components.utils.evalInSandbox(testArray, sandbox);
  Components.utils.evalInSandbox(testObject, sandbox);
  Components.utils.evalInSandbox(testHyphenated, sandbox);
  Components.utils.evalInSandbox(testLet, sandbox);

  do_print("Running tests with dbgObject");
  runChecks(dbgObject, null);

  do_print("Running tests with dbgEnv");
  runChecks(null, dbgEnv);

}

function runChecks(dbgObject, dbgEnv) {
  do_print("Test that suggestions are given for 'this'");
  let results = JSPropertyProvider(dbgObject, dbgEnv, "t");
  test_has_result(results, "this");

  if (dbgObject != null) {
    do_print("Test that suggestions are given for 'this.'");
    results = JSPropertyProvider(dbgObject, dbgEnv, "this.");
    test_has_result(results, "testObject");

    do_print("Test that no suggestions are given for 'this.this'");
    results = JSPropertyProvider(dbgObject, dbgEnv, "this.this");
    test_has_no_results(results);
  }

  do_print("Testing lexical scope issues (Bug 1207868)");
  results = JSPropertyProvider(dbgObject, dbgEnv, "foobar");
  test_has_result(results, "foobar");

  results = JSPropertyProvider(dbgObject, dbgEnv, "foobar.");
  test_has_result(results, "a");

  results = JSPropertyProvider(dbgObject, dbgEnv, "blargh");
  test_has_result(results, "blargh");

  results = JSPropertyProvider(dbgObject, dbgEnv, "blargh.");
  test_has_result(results, "a");

  do_print("Test that suggestions are given for 'foo[n]' where n is an integer.");
  results = JSPropertyProvider(dbgObject, dbgEnv, "testArray[0].");
  test_has_result(results, "propA");

  do_print("Test that suggestions are given for multidimensional arrays.");
  results = JSPropertyProvider(dbgObject, dbgEnv, "testArray[2][0].");
  test_has_result(results, "propE");

  do_print("Test that suggestions are given for nested arrays.");
  results = JSPropertyProvider(dbgObject, dbgEnv, "testArray[1].propC[0].");
  test_has_result(results, "indexOf");

  do_print("Test that suggestions are given for literal arrays.");
  results = JSPropertyProvider(dbgObject, dbgEnv, "[1,2,3].");
  test_has_result(results, "indexOf");

  do_print("Test that suggestions are given for literal arrays with newlines.");
  results = JSPropertyProvider(dbgObject, dbgEnv, "[1,2,3,\n4\n].");
  test_has_result(results, "indexOf");

  do_print("Test that suggestions are given for literal strings.");
  results = JSPropertyProvider(dbgObject, dbgEnv, "'foo'.");
  test_has_result(results, "charAt");
  results = JSPropertyProvider(dbgObject, dbgEnv, '"foo".');
  test_has_result(results, "charAt");
  results = JSPropertyProvider(dbgObject, dbgEnv, "`foo`.");
  test_has_result(results, "charAt");
  results = JSPropertyProvider(dbgObject, dbgEnv, "'[1,2,3]'.");
  test_has_result(results, "charAt");

  do_print("Test that suggestions are not given for syntax errors.");
  results = JSPropertyProvider(dbgObject, dbgEnv, "'foo\"");
  do_check_null(results);
  results = JSPropertyProvider(dbgObject, dbgEnv, "[1,',2]");
  do_check_null(results);
  results = JSPropertyProvider(dbgObject, dbgEnv, "'[1,2].");
  do_check_null(results);
  results = JSPropertyProvider(dbgObject, dbgEnv, "'foo'..");
  do_check_null(results);

  do_print("Test that suggestions are not given without a dot.");
  results = JSPropertyProvider(dbgObject, dbgEnv, "'foo'");
  test_has_no_results(results);
  results = JSPropertyProvider(dbgObject, dbgEnv, "[1,2,3]");
  test_has_no_results(results);
  results = JSPropertyProvider(dbgObject, dbgEnv, "[1,2,3].\n'foo'");
  test_has_no_results(results);

  do_print("Test that suggestions are not given for numeric literals.");
  results = JSPropertyProvider(dbgObject, dbgEnv, "1.");
  do_check_null(results);

  do_print("Test that suggestions are not given for index that's out of bounds.");
  results = JSPropertyProvider(dbgObject, dbgEnv, "testArray[10].");
  do_check_null(results);

  do_print("Test that no suggestions are given if an index is not numerical somewhere in the chain.");
  results = JSPropertyProvider(dbgObject, dbgEnv, "testArray[0]['propC'][0].");
  do_check_null(results);

  results = JSPropertyProvider(dbgObject, dbgEnv, "testObject['propA'][0].");
  do_check_null(results);

  results = JSPropertyProvider(dbgObject, dbgEnv, "testArray[0]['propC'].");
  do_check_null(results);

  results = JSPropertyProvider(dbgObject, dbgEnv, "testArray[][1].");
  do_check_null(results);

  do_print("Test that suggestions are not given if there is an hyphen in the chain.");
  results = JSPropertyProvider(dbgObject, dbgEnv, "testHyphenated['prop-A'].");
  do_check_null(results);
}

/**
 * A helper that ensures an empty array of results were found.
 * @param Object aResults
 *        The results returned by JSPropertyProvider.
 */
function test_has_no_results(aResults) {
  do_check_neq(aResults, null);
  do_check_eq(aResults.matches.length, 0);
}
/**
 * A helper that ensures (required) results were found.
 * @param Object aResults
 *        The results returned by JSPropertyProvider.
 * @param String aRequiredSuggestion
 *        A suggestion that must be found from the results.
 */
function test_has_result(aResults, aRequiredSuggestion) {
  do_check_neq(aResults, null);
  do_check_true(aResults.matches.length > 0);
  do_check_true(aResults.matches.indexOf(aRequiredSuggestion) !== -1);
}
