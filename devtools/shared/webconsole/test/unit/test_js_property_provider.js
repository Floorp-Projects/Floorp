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

  const testGenerators = `
  // Test with generator using a named function.
  function* genFunc() {
    for (let i = 0; i < 10; i++) {
      yield i;
    }
  }
  let gen1 = genFunc();
  gen1.next();

  // Test with generator using an anonymous function.
  let gen2 = (function* () {
    for (let i = 0; i < 10; i++) {
      yield i;
    }
  })();`;

  let sandbox = Components.utils.Sandbox("http://example.com");
  let dbg = new Debugger();
  let dbgObject = dbg.addDebuggee(sandbox);
  let dbgEnv = dbgObject.asEnvironment();
  Components.utils.evalInSandbox(testArray, sandbox);
  Components.utils.evalInSandbox(testObject, sandbox);
  Components.utils.evalInSandbox(testHyphenated, sandbox);
  Components.utils.evalInSandbox(testLet, sandbox);
  Components.utils.evalInSandbox(testGenerators, sandbox);

  info("Running tests with dbgObject");
  runChecks(dbgObject, null, sandbox);

  info("Running tests with dbgEnv");
  runChecks(null, dbgEnv, sandbox);
}

function runChecks(dbgObject, dbgEnv, sandbox) {
  info("Test that suggestions are given for 'this'");
  let results = JSPropertyProvider(dbgObject, dbgEnv, "t");
  test_has_result(results, "this");

  if (dbgObject != null) {
    info("Test that suggestions are given for 'this.'");
    results = JSPropertyProvider(dbgObject, dbgEnv, "this.");
    test_has_result(results, "testObject");

    info("Test that no suggestions are given for 'this.this'");
    results = JSPropertyProvider(dbgObject, dbgEnv, "this.this");
    test_has_no_results(results);
  }

  info("Testing lexical scope issues (Bug 1207868)");
  results = JSPropertyProvider(dbgObject, dbgEnv, "foobar");
  test_has_result(results, "foobar");

  results = JSPropertyProvider(dbgObject, dbgEnv, "foobar.");
  test_has_result(results, "a");

  results = JSPropertyProvider(dbgObject, dbgEnv, "blargh");
  test_has_result(results, "blargh");

  results = JSPropertyProvider(dbgObject, dbgEnv, "blargh.");
  test_has_result(results, "a");

  info("Test that suggestions are given for 'foo[n]' where n is an integer.");
  results = JSPropertyProvider(dbgObject, dbgEnv, "testArray[0].");
  test_has_result(results, "propA");

  info("Test that suggestions are given for multidimensional arrays.");
  results = JSPropertyProvider(dbgObject, dbgEnv, "testArray[2][0].");
  test_has_result(results, "propE");

  info("Test that suggestions are given for nested arrays.");
  results = JSPropertyProvider(dbgObject, dbgEnv, "testArray[1].propC[0].");
  test_has_result(results, "indexOf");

  info("Test that suggestions are given for literal arrays.");
  results = JSPropertyProvider(dbgObject, dbgEnv, "[1,2,3].");
  test_has_result(results, "indexOf");

  info("Test that suggestions are given for literal arrays with newlines.");
  results = JSPropertyProvider(dbgObject, dbgEnv, "[1,2,3,\n4\n].");
  test_has_result(results, "indexOf");

  info("Test that suggestions are given for literal strings.");
  results = JSPropertyProvider(dbgObject, dbgEnv, "'foo'.");
  test_has_result(results, "charAt");
  results = JSPropertyProvider(dbgObject, dbgEnv, '"foo".');
  test_has_result(results, "charAt");
  results = JSPropertyProvider(dbgObject, dbgEnv, "`foo`.");
  test_has_result(results, "charAt");
  results = JSPropertyProvider(dbgObject, dbgEnv, "'[1,2,3]'.");
  test_has_result(results, "charAt");

  info("Test that suggestions are not given for syntax errors.");
  results = JSPropertyProvider(dbgObject, dbgEnv, "'foo\"");
  Assert.equal(null, results);
  results = JSPropertyProvider(dbgObject, dbgEnv, "[1,',2]");
  Assert.equal(null, results);
  results = JSPropertyProvider(dbgObject, dbgEnv, "'[1,2].");
  Assert.equal(null, results);
  results = JSPropertyProvider(dbgObject, dbgEnv, "'foo'..");
  Assert.equal(null, results);

  info("Test that suggestions are not given without a dot.");
  results = JSPropertyProvider(dbgObject, dbgEnv, "'foo'");
  test_has_no_results(results);
  results = JSPropertyProvider(dbgObject, dbgEnv, "[1,2,3]");
  test_has_no_results(results);
  results = JSPropertyProvider(dbgObject, dbgEnv, "[1,2,3].\n'foo'");
  test_has_no_results(results);

  info("Test that suggestions are not given for numeric literals.");
  results = JSPropertyProvider(dbgObject, dbgEnv, "1.");
  Assert.equal(null, results);

  info("Test that suggestions are not given for index that's out of bounds.");
  results = JSPropertyProvider(dbgObject, dbgEnv, "testArray[10].");
  Assert.equal(null, results);

  info("Test that no suggestions are given if an index is not numerical "
       + "somewhere in the chain.");
  results = JSPropertyProvider(dbgObject, dbgEnv, "testArray[0]['propC'][0].");
  Assert.equal(null, results);

  results = JSPropertyProvider(dbgObject, dbgEnv, "testObject['propA'][0].");
  Assert.equal(null, results);

  results = JSPropertyProvider(dbgObject, dbgEnv, "testArray[0]['propC'].");
  Assert.equal(null, results);

  results = JSPropertyProvider(dbgObject, dbgEnv, "testArray[][1].");
  Assert.equal(null, results);

  info("Test that suggestions are not given if there is an hyphen in the chain.");
  results = JSPropertyProvider(dbgObject, dbgEnv, "testHyphenated['prop-A'].");
  Assert.equal(null, results);

  info("Test that we have suggestions for generators.");
  let gen1Result = Components.utils.evalInSandbox("gen1.next().value", sandbox);
  results = JSPropertyProvider(dbgObject, dbgEnv, "gen1.");
  test_has_result(results, "next");
  info("Test that the generator next() was not executed");
  let gen1NextResult = Components.utils.evalInSandbox("gen1.next().value", sandbox);
  Assert.equal(gen1Result + 1, gen1NextResult);

  info("Test with an anonymous generator.");
  let gen2Result = Components.utils.evalInSandbox("gen2.next().value", sandbox);
  results = JSPropertyProvider(dbgObject, dbgEnv, "gen2.");
  test_has_result(results, "next");
  let gen2NextResult = Components.utils.evalInSandbox("gen2.next().value", sandbox);
  Assert.equal(gen2Result + 1, gen2NextResult);
}

/**
 * A helper that ensures an empty array of results were found.
 * @param Object results
 *        The results returned by JSPropertyProvider.
 */
function test_has_no_results(results) {
  Assert.notEqual(results, null);
  Assert.equal(results.matches.length, 0);
}
/**
 * A helper that ensures (required) results were found.
 * @param Object results
 *        The results returned by JSPropertyProvider.
 * @param String requiredSuggestion
 *        A suggestion that must be found from the results.
 */
function test_has_result(results, requiredSuggestion) {
  Assert.notEqual(results, null);
  Assert.ok(results.matches.length > 0);
  Assert.ok(results.matches.indexOf(requiredSuggestion) !== -1);
}
