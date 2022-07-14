/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function() {
  // Test basic import.
  let ns = ChromeUtils.importESModule("resource://test/es6module.js");
  Assert.equal(ns.loadCount, 1);
  Assert.equal(ns.value, 2);

  // Test re-import of the same module.
  let ns2 = ChromeUtils.importESModule("resource://test/es6module.js");
  Assert.equal(ns.loadCount, 1);
  Assert.equal(ns, ns2);

  // Test imports with absolute and relative URIs return the same thing.
  let ns3 = ChromeUtils.importESModule("resource://test/es6module_absolute.js");
  let ns4 = ChromeUtils.importESModule("resource://test/es6module_absolute2.js");
  Assert.ok(ns3.absoluteX === ns3.relativeX);
  Assert.ok(ns3.absoluteX === ns4.x);

  // Test load failure.
  testFailure("resource://test/es6module_not_found.js", {
    type: "Error",
    message: "Failed to load resource://test/es6module_not_found.js",
    fileName: "test_import_es6_modules.js",
    stack: "testFailure",
    lineNumber: "*",
    columnNumber: "*",
  });

  // Test load failure in import.
  testFailure("resource://test/es6module_missing_import.js", {
    type: "Error",
    message: "Failed to load resource://test/es6module_not_found2.js",
    fileName: "test_import_es6_modules.js",
    stack: "testFailure",
    lineNumber: "*",
    columnNumber: "*",
  });

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

  // Test import error.
  testFailure("resource://test/es6module_import_error.js", {
    type: "SyntaxError",
    fileName: "resource://test/es6module_import_error.js",
    lineNumber: 1,
    columnNumber: 9,
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

  // Test dynamic import is not supported.
  ns = ChromeUtils.importESModule("resource://test/es6module_dynamic_import.js");
  const e = await ns.result;
  checkException(e, {
    type: "Error",
    message: "not supported",
    fileName: "resource://test/es6module_dynamic_import.js",
    lineNumber: 5,
    columnNumber: 1,
  });
});

function testFailure(url, expected) {
  let threw = false;
  let exception;
  let importLine, importColumn;
  try {
    // Get the line/column for ChromeUtils.importESModule.
    // lineNumber/columnNumber value with "*" in `expected` points the
    // line/column.
    let e = new Error();
    importLine = e.lineNumber + 3;
    importColumn = 17;
    ChromeUtils.importESModule(url);
  } catch (e) {
    threw = true;
    exception = e;
  }

  Assert.ok(threw, "Error should be thrown");

  checkException(exception, expected, importLine, importColumn);

  return exception;
}

function checkException(exception, expected, importLine, importColumn) {
  if ("type" in expected) {
    Assert.equal(exception.constructor.name, expected.type, "error type");
  }
  if ("message" in expected) {
    Assert.ok(exception.message.includes(expected.message),
              `Message "${exception.message}" should contain "${expected.message}"`);
  }
  if ("stack" in expected) {
    Assert.ok(exception.stack.includes(expected.stack),
              `Stack "${exception.stack}" should contain "${expected.stack}"`);
  }
  if ("fileName" in expected) {
    Assert.ok(exception.fileName.includes(expected.fileName),
              `fileName "${exception.fileName}" should contain "${expected.fileName}"`);
  }
  if ("lineNumber" in expected) {
    let expectedLine = expected.lineNumber;
    if (expectedLine === "*") {
      expectedLine = importLine;
    }
    Assert.equal(exception.lineNumber, expectedLine, "lineNumber");
  }
  if ("columnNumber" in expected) {
    let expectedColumn = expected.columnNumber;
    if (expectedColumn === "*") {
      expectedColumn = importColumn;
    }
    Assert.equal(exception.columnNumber, expectedColumn, "columnNumber");
  }
}
