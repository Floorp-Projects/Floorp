// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/

"use strict";
const { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm");
const {
  FallibleJSPropertyProvider: JSPropertyProvider,
} = require("devtools/shared/webconsole/js-property-provider");

const { addDebuggerToGlobal } = ChromeUtils.import(
  "resource://gre/modules/jsdebugger.jsm"
);
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
  Cu.evalInSandbox(
    `
    const hello = Object.create(null, Object.getOwnPropertyDescriptors({world: 1}));
    String.prototype.hello = hello;
    Number.prototype.hello = hello;
    Array.prototype.hello = hello;
  `,
    sandbox
  );
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

function runChecks(dbgObject, environment, sandbox) {
  const propertyProvider = (inputValue, options) =>
    JSPropertyProvider({
      dbgObject,
      environment,
      inputValue,
      ...options,
    });

  info("Test that suggestions are given for 'this'");
  let results = propertyProvider("t");
  test_has_result(results, "this");

  if (dbgObject != null) {
    info("Test that suggestions are given for 'this.'");
    results = propertyProvider("this.");
    test_has_result(results, "testObject");

    info("Test that suggestions are given for '(this).'");
    results = propertyProvider("(this).");
    test_has_result(results, "testObject");

    info("Test that suggestions are given for deep 'this' properties access");
    results = propertyProvider("(this).testObject.propA.");
    test_has_result(results, "shift");

    results = propertyProvider("(this).testObject.propA[");
    test_has_result(results, `"shift"`);

    results = propertyProvider("(this)['testObject']['propA'][");
    test_has_result(results, `"shift"`);

    results = propertyProvider("(this).testObject['propA'].");
    test_has_result(results, "shift");

    info("Test that no suggestions are given for 'this.this'");
    results = propertyProvider("this.this");
    test_has_no_results(results);
  }

  info("Test that suggestions are given for 'globalThis'");
  results = propertyProvider("g");
  test_has_result(results, "globalThis");

  info("Test that suggestions are given for 'globalThis.'");
  results = propertyProvider("globalThis.");
  test_has_result(results, "testObject");

  info("Test that suggestions are given for '(globalThis).'");
  results = propertyProvider("(globalThis).");
  test_has_result(results, "testObject");

  info(
    "Test that suggestions are given for deep 'globalThis' properties access"
  );
  results = propertyProvider("(globalThis).testObject.propA.");
  test_has_result(results, "shift");

  results = propertyProvider("(globalThis).testObject.propA[");
  test_has_result(results, `"shift"`);

  results = propertyProvider("(globalThis)['testObject']['propA'][");
  test_has_result(results, `"shift"`);

  results = propertyProvider("(globalThis).testObject['propA'].");
  test_has_result(results, "shift");

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

  results = propertyProvider("[1,2,3].h");
  test_has_result(results, "hello");

  results = propertyProvider("[1,2,3].hello.w");
  test_has_result(results, "world");

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
  results = propertyProvider('`foo " doc`.');
  test_has_result(results, "charAt");
  results = propertyProvider("`foo ' doc`.");
  test_has_result(results, "charAt");
  results = propertyProvider("'[1,2,3]'.");
  test_has_result(results, "charAt");
  results = propertyProvider("'foo'.h");
  test_has_result(results, "hello");
  results = propertyProvider("'foo'.hello.w");
  test_has_result(results, "world");

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

  info("Test that suggestions are not given for index that's out of bounds.");
  results = propertyProvider("testArray[10].");
  Assert.equal(null, results);

  info("Test that invalid element access syntax does not return anything");
  results = propertyProvider("testArray[][1].");
  Assert.equal(null, results);

  info("Test that deep element access works.");
  results = propertyProvider("testObject['propA'][0].");
  test_has_result(results, "propB");

  results = propertyProvider("testArray[1]['propC'].");
  test_has_result(results, "shift");

  results = propertyProvider("testArray[1].propC[0][");
  test_has_result(results, `"trim"`);

  results = propertyProvider("testArray[1].propC[0].");
  test_has_result(results, "trim");

  info(
    "Test that suggestions are displayed when variable is wrapped in parens"
  );
  results = propertyProvider("(testObject)['propA'][0].");
  test_has_result(results, "propB");

  results = propertyProvider("(testArray)[1]['propC'].");
  test_has_result(results, "shift");

  results = propertyProvider("(testArray)[1].propC[0][");
  test_has_result(results, `"trim"`);

  results = propertyProvider("(testArray)[1].propC[0].");
  test_has_result(results, "trim");

  info("Test that suggestions are given if there is an hyphen in the chain.");
  results = propertyProvider("testHyphenated['prop-A'].");
  test_has_result(results, "trim");

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

  info(
    "Test that getters are not executed if authorizedEvaluations is undefined"
  );
  results = propertyProvider("testGetters.x.");
  Assert.deepEqual(results, {
    isUnsafeGetter: true,
    getterPath: ["testGetters", "x"],
  });

  results = propertyProvider("testGetters.x[");
  Assert.deepEqual(results, {
    isUnsafeGetter: true,
    getterPath: ["testGetters", "x"],
  });

  results = propertyProvider("testGetters.x.hell");
  Assert.deepEqual(results, {
    isUnsafeGetter: true,
    getterPath: ["testGetters", "x"],
  });

  results = propertyProvider("testGetters.x['hell");
  Assert.deepEqual(results, {
    isUnsafeGetter: true,
    getterPath: ["testGetters", "x"],
  });

  info(
    "Test that getters are not executed if authorizedEvaluations does not match"
  );
  results = propertyProvider("testGetters.x.", { authorizedEvaluations: [] });
  Assert.deepEqual(results, {
    isUnsafeGetter: true,
    getterPath: ["testGetters", "x"],
  });

  results = propertyProvider("testGetters.x.", {
    authorizedEvaluations: [["testGetters"]],
  });
  Assert.deepEqual(results, {
    isUnsafeGetter: true,
    getterPath: ["testGetters", "x"],
  });

  results = propertyProvider("testGetters.x.", {
    authorizedEvaluations: [["testGtrs", "x"]],
  });
  Assert.deepEqual(results, {
    isUnsafeGetter: true,
    getterPath: ["testGetters", "x"],
  });

  results = propertyProvider("testGetters.x.", {
    authorizedEvaluations: [["x"]],
  });
  Assert.deepEqual(results, {
    isUnsafeGetter: true,
    getterPath: ["testGetters", "x"],
  });

  info("Test that deep getter property access returns intermediate getters");
  results = propertyProvider("testGetters.y.y.");
  Assert.deepEqual(results, {
    isUnsafeGetter: true,
    getterPath: ["testGetters", "y"],
  });

  results = propertyProvider("testGetters['y'].y.");
  Assert.deepEqual(results, {
    isUnsafeGetter: true,
    getterPath: ["testGetters", "y"],
  });

  results = propertyProvider("testGetters['y']['y'].");
  Assert.deepEqual(results, {
    isUnsafeGetter: true,
    getterPath: ["testGetters", "y"],
  });

  results = propertyProvider("testGetters.y['y'].");
  Assert.deepEqual(results, {
    isUnsafeGetter: true,
    getterPath: ["testGetters", "y"],
  });

  info("Test that deep getter property access invoke intermediate getters");
  results = propertyProvider("testGetters.y.y.", {
    authorizedEvaluations: [["testGetters", "y"]],
  });
  Assert.deepEqual(results, {
    isUnsafeGetter: true,
    getterPath: ["testGetters", "y", "y"],
  });

  results = propertyProvider("testGetters['y'].y.", {
    authorizedEvaluations: [["testGetters", "y"]],
  });
  Assert.deepEqual(results, {
    isUnsafeGetter: true,
    getterPath: ["testGetters", "y", "y"],
  });

  results = propertyProvider("testGetters['y']['y'].", {
    authorizedEvaluations: [["testGetters", "y"]],
  });
  Assert.deepEqual(results, {
    isUnsafeGetter: true,
    getterPath: ["testGetters", "y", "y"],
  });

  results = propertyProvider("testGetters.y['y'].", {
    authorizedEvaluations: [["testGetters", "y"]],
  });
  Assert.deepEqual(results, {
    isUnsafeGetter: true,
    getterPath: ["testGetters", "y", "y"],
  });

  info(
    "Test that getters are executed if matching an authorizedEvaluation element"
  );
  results = propertyProvider("testGetters.x.", {
    authorizedEvaluations: [["testGetters", "x"]],
  });
  test_has_exact_results(results, ["hello", "world"]);
  Assert.ok(Object.keys(results).includes("isUnsafeGetter") === false);
  Assert.ok(Object.keys(results).includes("getterPath") === false);

  results = propertyProvider("testGetters.x.", {
    authorizedEvaluations: [["testGetters", "x"], ["y"]],
  });
  test_has_exact_results(results, ["hello", "world"]);
  Assert.ok(Object.keys(results).includes("isUnsafeGetter") === false);
  Assert.ok(Object.keys(results).includes("getterPath") === false);

  info("Test that executing getters filters with provided string");
  results = propertyProvider("testGetters.x.hell", {
    authorizedEvaluations: [["testGetters", "x"]],
  });
  test_has_exact_results(results, ["hello"]);

  results = propertyProvider("testGetters.x['hell", {
    authorizedEvaluations: [["testGetters", "x"]],
  });
  test_has_exact_results(results, ["'hello'"]);

  info(
    "Test children getters are not executed if not included in authorizedEvaluation"
  );
  results = propertyProvider("testGetters.y.y.", {
    authorizedEvaluations: [["testGetters", "y", "y"]],
  });
  Assert.deepEqual(results, {
    isUnsafeGetter: true,
    getterPath: ["testGetters", "y"],
  });

  info(
    "Test children getters are executed if matching an authorizedEvaluation element"
  );
  results = propertyProvider("testGetters.y.y.", {
    authorizedEvaluations: [["testGetters", "y"], ["testGetters", "y", "y"]],
  });
  test_has_result(results, "trim");

  info("Test with number literals");
  results = propertyProvider("1.");
  Assert.ok(results === null, "Does not complete on possible floating number");

  results = propertyProvider("(1)..");
  Assert.ok(results === null, "Does not complete on invalid syntax");

  results = propertyProvider("(1.1.).");
  Assert.ok(results === null, "Does not complete on invalid syntax");

  results = propertyProvider("1..");
  test_has_result(results, "toFixed");

  results = propertyProvider("1 .");
  test_has_result(results, "toFixed");

  results = propertyProvider("1\n.");
  test_has_result(results, "toFixed");

  results = propertyProvider(".1.");
  test_has_result(results, "toFixed");

  results = propertyProvider("1[");
  test_has_result(results, `"toFixed"`);

  results = propertyProvider("1[toFixed");
  test_has_exact_results(results, [`"toFixed"`]);

  results = propertyProvider("1['toFixed");
  test_has_exact_results(results, ["'toFixed'"]);

  results = propertyProvider("1.1[");
  test_has_result(results, `"toFixed"`);

  results = propertyProvider("(1).");
  test_has_result(results, "toFixed");

  results = propertyProvider("(.1).");
  test_has_result(results, "toFixed");

  results = propertyProvider("(1.1).");
  test_has_result(results, "toFixed");

  results = propertyProvider("(1).toFixed");
  test_has_exact_results(results, ["toFixed"]);

  results = propertyProvider("(1)[");
  test_has_result(results, `"toFixed"`);

  results = propertyProvider("(1.1)[");
  test_has_result(results, `"toFixed"`);

  results = propertyProvider("(1)[toFixed");
  test_has_exact_results(results, [`"toFixed"`]);

  results = propertyProvider("(1)['toFixed");
  test_has_exact_results(results, ["'toFixed'"]);

  results = propertyProvider("(1).h");
  test_has_result(results, "hello");

  results = propertyProvider("(1).hello.w");
  test_has_result(results, "world");

  info("Test access on dot-notation invalid property name");
  results = propertyProvider("testHyphenated.prop");
  Assert.ok(
    !results.matches.has("prop-A"),
    "Does not return invalid property name on dot access"
  );

  results = propertyProvider("testHyphenated['prop");
  test_has_result(results, `'prop-A'`);

  results = propertyProvider(`//t`);
  Assert.ok(results === null, "Does not complete in inline comment");

  results = propertyProvider(`// t`);
  Assert.ok(
    results === null,
    "Does not complete in inline comment after space"
  );

  results = propertyProvider(`//I'm a comment\nt`);
  test_has_result(results, "testObject");

  results = propertyProvider(`1/t`);
  test_has_result(results, "testObject");

  results = propertyProvider(`/* t`);
  Assert.ok(results === null, "Does not complete in multiline comment");

  results = propertyProvider(`/*I'm\nt`);
  Assert.ok(
    results === null,
    "Does not complete in multiline comment after line break"
  );

  results = propertyProvider(`/*I'm a comment\n \t * /t`);
  Assert.ok(
    results === null,
    "Does not complete in multiline comment after line break and invalid comment end"
  );

  results = propertyProvider(`/*I'm a comment\n \t */t`);
  test_has_result(results, "testObject");

  results = propertyProvider(`/*I'm a comment\n \t */\n\nt`);
  test_has_result(results, "testObject");
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
