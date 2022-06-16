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
  let threw = false;
  try {
    ChromeUtils.importESModule("resource://test/es6module_not_found.js");
  } catch (e) {
    threw = true;
  }
  Assert.ok(threw);

  // Test execution failure.
  threw = false;
  let exception;
  try {
    ChromeUtils.importESModule("resource://test/es6module_throws.js");
  } catch (e) {
    exception = e;
    threw = true;
  }
  Assert.ok(threw);
  Assert.ok(exception.toString().includes("foobar"));

  // Test re-import throws the same exception.
  threw = false;
  let exception2;
  try {
    ChromeUtils.importESModule("resource://test/es6module_throws.js");
  } catch (e) {
    exception2 = e;
    threw = true;
  }
  Assert.ok(threw);
  Assert.equal(exception, exception2);

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
  threw = false;
  exception = undefined;
  try {
    ChromeUtils.importESModule("resource://test/es6module_top_level_await.js");
  } catch (e) {
    exception = e;
    threw = true;
  }
  Assert.ok(threw);
  Assert.ok(exception.message.includes("not supported"));
}
