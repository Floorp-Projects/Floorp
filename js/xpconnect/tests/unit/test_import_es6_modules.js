/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  // Test basic import.
  let ns = ChromeUtils.importESModule("resource://test/es6module.js");
  Assert.equal(ns.loadCount, 1);
  Assert.equal(ns.value, 2);

  // Test re-import of the same module.
  let ns2 = ChromeUtils.importESModule("resource://test/es6module.js");
  Assert.equal(ns.loadCount, 1);
  Assert.equal(ns, ns2);

  // Test load failure.
  testFailure("resource://test/es6module_not_found.js");

  // Test load failure in import.
  testFailure("resource://test/es6module_missing_import.js");

  // Test parse error.
  testFailure("resource://test/es6module_parse_error.js", {
    type: "SyntaxError",
    fileName: "resource://test/es6module_parse_error.js",
    stack: "testFailure",
    lineNumber: 1,
    columnNumber: 5,
  });

  // Test parse error in import.
  testFailure("resource://test/es6module_parse_error_in_import.js", {
    type: "SyntaxError",
    fileName: "resource://test/es6module_parse_error.js",
    stack: "testFailure",
    lineNumber: 1,
    columnNumber: 5,
  });

  // Test execution failure.
  let exception1 = testFailure("resource://test/es6module_throws.js", {
    type: "Error",
    message: "foobar",
    stack: "throwFunction",
    fileName: "resource://test/es6module_throws.js",
    lineNumber: 2,
    columnNumber: 9,
  });

  // Test re-import throws the same exception.
  let exception2 = testFailure("resource://test/es6module_throws.js", {
    type: "Error",
    message: "foobar",
    stack: "throwFunction",
    fileName: "resource://test/es6module_throws.js",
    lineNumber: 2,
    columnNumber: 9,
  });
  Assert.ok(exception1 === exception2);

  // Test loading cyclic module graph.
  ns = ChromeUtils.importESModule("resource://test/es6module_cycle_a.js");
  Assert.ok(ns.loaded);
  Assert.equal(ns.getValueFromB(), "b");
  ns = ChromeUtils.importESModule("resource://test/es6module_cycle_b.js");
  Assert.ok(ns.loaded);
  Assert.equal(ns.getValueFromC(), "c");
  ns = ChromeUtils.importESModule("resource://test/es6module_cycle_c.js");
  Assert.ok(ns.loaded);
  Assert.equal(ns.getValueFromA(), "a");

  // Test top-level await is not supported.
  testFailure("resource://test/es6module_top_level_await.js", {
    type: "SyntaxError",
    message: "not supported",
    stack: "testFailure",
    fileName: "resource://test/es6module_top_level_await.js",
    lineNumber: 1,
    columnNumber: 0,
  });
}

function testFailure(url, expected) {
  let threw = false;
  let exception;
  try {
    ChromeUtils.importESModule(url);
  } catch (e) {
    threw = true;
    exception = e;
  }

  Assert.ok(threw);

  if (expected) {
    if ("type" in expected) {
      Assert.equal(exception.constructor.name, expected.type, "error type");
    }
    if ("message" in expected) {
      Assert.ok(exception.message.includes(expected.message), "message");
    }
    if ("stack" in expected) {
      Assert.ok(exception.stack.includes(expected.stack), "stack");
    }
    if ("fileName" in expected) {
      Assert.equal(exception.fileName, expected.fileName, "fileName");
    }
    if ("lineNumber" in expected) {
      Assert.equal(exception.lineNumber, expected.lineNumber, "lineNumber");
    }
    if ("columnNumber" in expected) {
      Assert.equal(exception.columnNumber, expected.columnNumber, "columnNumber");
    }
  }

  return exception;
}
