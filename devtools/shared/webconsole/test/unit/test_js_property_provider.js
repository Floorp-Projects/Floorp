/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/

"use strict";
const { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm", {});
const { FallibleJSPropertyProvider: JSPropertyProvider } =
  require("devtools/shared/webconsole/js-property-provider");

ChromeUtils.import("resource://gre/modules/jsdebugger.jsm");
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

  const testGetters = `
    var testGetters = {
      get x() {
        return Object.create(null, Object.getOwnPropertyDescriptors({
          hello: "",
          world: "",
        }));
      },
      get y() {
        return Object.create(null, Object.getOwnPropertyDescriptors({
          get y() {
            return "plop";
          },
        }));
      }
    };
  `;

  const sandbox = Cu.Sandbox("http://example.com");
  const dbg = new Debugger();
  const dbgObject = dbg.addDebuggee(sandbox);
  const dbgEnv = dbgObject.asEnvironment();
  Cu.evalInSandbox(testArray, sandbox);
  Cu.evalInSandbox(testObject, sandbox);
  Cu.evalInSandbox(testHyphenated, sandbox);
  Cu.evalInSandbox(testLet, sandbox);
  Cu.evalInSandbox(testGenerators, sandbox);
  Cu.evalInSandbox(testGetters, sandbox);

  info("Running tests with dbgObject");
  runChecks(dbgObject, null, sandbox);

  info("Running tests with dbgEnv");
  runChecks(null, dbgEnv, sandbox);
}

function runChecks(dbgObject, dbgEnv, sandbox) {
  const propertyProvider = (...args) => JSPropertyProvider(dbgObject, dbgEnv, ...args);

  info("Test that suggestions are given for 'this'");
  let results = propertyProvider("t");
  test_has_result(results, "this");

  if (dbgObject != null) {
    info("Test that suggestions are given for 'this.'");
    results = propertyProvider("this.");
    test_has_result(results, "testObject");

    info("Test that no suggestions are given for 'this.this'");
    results = propertyProvider("this.this");
    test_has_no_results(results);
  }

  info("Testing lexical scope issues (Bug 1207868)");
  results = propertyProvider("foobar");
  test_has_result(results, "foobar");

  results = propertyProvider("foobar.");
  test_has_result(results, "a");

  results = propertyProvider("blargh");
  test_has_result(results, "blargh");

  results = propertyProvider("blargh.");
  test_has_result(results, "a");

  info("Test that suggestions are given for 'foo[n]' where n is an integer.");
  results = propertyProvider("testArray[0].");
  test_has_result(results, "propA");

  info("Test that suggestions are given for multidimensional arrays.");
  results = propertyProvider("testArray[2][0].");
  test_has_result(results, "propE");

  info("Test that suggestions are given for nested arrays.");
  results = propertyProvider("testArray[1].propC[0].");
  test_has_result(results, "indexOf");

  info("Test that suggestions are given for literal arrays.");
  results = propertyProvider("[1,2,3].");
  test_has_result(results, "indexOf");

  info("Test that suggestions are given for literal arrays with newlines.");
  results = propertyProvider("[1,2,3,\n4\n].");
  test_has_result(results, "indexOf");

  info("Test that suggestions are given for literal strings.");
  results = propertyProvider("'foo'.");
  test_has_result(results, "charAt");
  results = propertyProvider('"foo".');
  test_has_result(results, "charAt");
  results = propertyProvider("`foo`.");
  test_has_result(results, "charAt");
  results = propertyProvider("`foo doc`.");
  test_has_result(results, "charAt");
  results = propertyProvider("`foo \" doc`.");
  test_has_result(results, "charAt");
  results = propertyProvider("`foo \' doc`.");
  test_has_result(results, "charAt");
  results = propertyProvider("'[1,2,3]'.");
  test_has_result(results, "charAt");

  info("Test that suggestions are not given for syntax errors.");
  results = propertyProvider("'foo\"");
  Assert.equal(null, results);
  results = propertyProvider("'foo d");
  Assert.equal(null, results);
  results = propertyProvider(`"foo d`);
  Assert.equal(null, results);
  results = propertyProvider("`foo d");
  Assert.equal(null, results);
  results = propertyProvider("[1,',2]");
  Assert.equal(null, results);
  results = propertyProvider("'[1,2].");
  Assert.equal(null, results);
  results = propertyProvider("'foo'..");
  Assert.equal(null, results);

  info("Test that suggestions are not given without a dot.");
  results = propertyProvider("'foo'");
  test_has_no_results(results);
  results = propertyProvider("`foo`");
  test_has_no_results(results);
  results = propertyProvider("[1,2,3]");
  test_has_no_results(results);
  results = propertyProvider("[1,2,3].\n'foo'");
  test_has_no_results(results);

  info("Test that suggestions are not given for numeric literals.");
  results = propertyProvider("1.");
  Assert.equal(null, results);

  info("Test that suggestions are not given for index that's out of bounds.");
  results = propertyProvider("testArray[10].");
  Assert.equal(null, results);

  info("Test that no suggestions are given if an index is not numerical "
       + "somewhere in the chain.");
  results = propertyProvider("testArray[0]['propC'][0].");
  Assert.equal(null, results);

  results = propertyProvider("testObject['propA'][0].");
  Assert.equal(null, results);

  results = propertyProvider("testArray[0]['propC'].");
  Assert.equal(null, results);

  results = propertyProvider("testArray[][1].");
  Assert.equal(null, results);

  info("Test that suggestions are not given if there is an hyphen in the chain.");
  results = propertyProvider("testHyphenated['prop-A'].");
  Assert.equal(null, results);

  info("Test that we have suggestions for generators.");
  const gen1Result = Cu.evalInSandbox("gen1.next().value", sandbox);
  results = propertyProvider("gen1.");
  test_has_result(results, "next");
  info("Test that the generator next() was not executed");
  const gen1NextResult = Cu.evalInSandbox("gen1.next().value", sandbox);
  Assert.equal(gen1Result + 1, gen1NextResult);

  info("Test with an anonymous generator.");
  const gen2Result = Cu.evalInSandbox("gen2.next().value", sandbox);
  results = propertyProvider("gen2.");
  test_has_result(results, "next");
  const gen2NextResult = Cu.evalInSandbox("gen2.next().value", sandbox);
  Assert.equal(gen2Result + 1, gen2NextResult);

  info("Test that getters are not executed if invokeUnsafeGetter is undefined");
  results = propertyProvider("testGetters.x.");
  Assert.deepEqual(results, {isUnsafeGetter: true, getterName: "x"});

  results = propertyProvider("testGetters.x[");
  Assert.deepEqual(results, {isUnsafeGetter: true, getterName: "x"});

  results = propertyProvider("testGetters.x.hell");
  Assert.deepEqual(results, {isUnsafeGetter: true, getterName: "x"});

  results = propertyProvider("testGetters.x['hell");
  Assert.deepEqual(results, {isUnsafeGetter: true, getterName: "x"});

  info("Test that deep getter property access does not return intermediate getters");
  results = propertyProvider("testGetters.y.y.");
  Assert.ok(results === null);

  results = propertyProvider("testGetters['y'].y.");
  Assert.ok(results === null);

  results = propertyProvider("testGetters['y']['y'].");
  Assert.ok(results === null);

  results = propertyProvider("testGetters.y['y'].");
  Assert.ok(results === null);

  info("Test that getters are executed if invokeUnsafeGetter is true");
  results = propertyProvider("testGetters.x.", undefined, true);
  test_has_exact_results(results, ["hello", "world"]);
  Assert.ok(Object.keys(results).includes("isUnsafeGetter") === false);
  Assert.ok(Object.keys(results).includes("getterName") === false);

  info("Test that executing getters filters with provided string");
  results = propertyProvider("testGetters.x.hell", undefined, true);
  test_has_exact_results(results, ["hello"]);

  results = propertyProvider("testGetters.x['hell", undefined, true);
  test_has_exact_results(results, ["'hello'"]);

  info("Test that children getters are executed if invokeUnsafeGetter is true");
  results = propertyProvider("testGetters.y.y.", undefined, true);
  test_has_result(results, "trim");
}

/**
 * A helper that ensures an empty array of results were found.
 * @param Object results
 *        The results returned by JSPropertyProvider.
 */
function test_has_no_results(results) {
  Assert.notEqual(results, null);
  Assert.equal(results.matches.size, 0);
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
  Assert.ok(results.matches.size > 0);
  Assert.ok(results.matches.has(requiredSuggestion));
}

/**
 * A helper that ensures results are the expected ones.
 * @param Object results
 *        The results returned by JSPropertyProvider.
 * @param Array expectedMatches
 *        An array of the properties that should be returned by JsPropertyProvider.
 */
function test_has_exact_results(results, expectedMatches) {
  Assert.deepEqual([...results.matches], expectedMatches);
}
