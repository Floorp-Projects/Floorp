"use strict";

Cu.import("resource://shield-recipe-client/lib/Storage.jsm", this);

const fakeSandbox = {Promise};
const store1 = Storage.makeStorage("prefix1", fakeSandbox);
const store2 = Storage.makeStorage("prefix2", fakeSandbox);

add_task(function* () {
  // Make sure values return null before being set
  Assert.equal(yield store1.getItem("key"), null);
  Assert.equal(yield store2.getItem("key"), null);

  // Set values to check
  yield store1.setItem("key", "value1");
  yield store2.setItem("key", "value2");

  // Check that they are available
  Assert.equal(yield store1.getItem("key"), "value1");
  Assert.equal(yield store2.getItem("key"), "value2");

  // Remove them, and check they are gone
  yield store1.removeItem("key");
  yield store2.removeItem("key");
  Assert.equal(yield store1.getItem("key"), null);
  Assert.equal(yield store2.getItem("key"), null);

  // Check that numbers are stored as numbers (not strings)
  yield store1.setItem("number", 42);
  Assert.equal(yield store1.getItem("number"), 42);

  // Check complex types work
  const complex = {a: 1, b: [2, 3], c: {d: 4}};
  yield store1.setItem("complex", complex);
  Assert.deepEqual(yield store1.getItem("complex"), complex);

  // Check that clearing the storage removes data from multiple
  // prefixes.
  yield store1.setItem("removeTest", 1);
  yield store2.setItem("removeTest", 2);
  Assert.equal(yield store1.getItem("removeTest"), 1);
  Assert.equal(yield store2.getItem("removeTest"), 2);
  yield Storage.clearAllStorage();
  Assert.equal(yield store1.getItem("removeTest"), null);
  Assert.equal(yield store2.getItem("removeTest"), null);
});
