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
  testFailure("resource://test/es6module_parse_error.js", "SyntaxError");

  // Test parse error in import.
  testFailure("resource://test/es6module_parse_error_in_import.js", "SyntaxError");

  // Test execution failure.
  let exception1 = testFailure("resource://test/es6module_throws.js", "foobar");

  // Test re-import throws the same exception.
  let exception2 = testFailure("resource://test/es6module_throws.js", "foobar");
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
  testFailure("resource://test/es6module_top_level_await.js", "not supported");
}

function testFailure(url, exceptionPart) {
  let threw = false;
  let exception;
  try {
    ChromeUtils.importESModule(url);
  } catch (e) {
    threw = true;
    exception = e;
  }

  Assert.ok(threw);
  if (exceptionPart) {
    Assert.ok(exception.toString().includes(exceptionPart));
  }

  return exception;
}
