"use strict";

Cu.import("resource://shield-recipe-client/lib/Storage.jsm", this);
Cu.import("resource://shield-recipe-client/lib/SandboxManager.jsm", this);

add_task(async function() {
  const store1 = new Storage("prefix1");
  const store2 = new Storage("prefix2");

  // Make sure values return null before being set
  Assert.equal(await store1.getItem("key"), null);
  Assert.equal(await store2.getItem("key"), null);

  // Set values to check
  await store1.setItem("key", "value1");
  await store2.setItem("key", "value2");

  // Check that they are available
  Assert.equal(await store1.getItem("key"), "value1");
  Assert.equal(await store2.getItem("key"), "value2");

  // Remove them, and check they are gone
  await store1.removeItem("key");
  await store2.removeItem("key");
  Assert.equal(await store1.getItem("key"), null);
  Assert.equal(await store2.getItem("key"), null);

  // Check that numbers are stored as numbers (not strings)
  await store1.setItem("number", 42);
  Assert.equal(await store1.getItem("number"), 42);

  // Check complex types work
  const complex = {a: 1, b: [2, 3], c: {d: 4}};
  await store1.setItem("complex", complex);
  Assert.deepEqual(await store1.getItem("complex"), complex);

  // Check that clearing the storage removes data from multiple
  // prefixes.
  await store1.setItem("removeTest", 1);
  await store2.setItem("removeTest", 2);
  Assert.equal(await store1.getItem("removeTest"), 1);
  Assert.equal(await store2.getItem("removeTest"), 2);
  await Storage.clearAllStorage();
  Assert.equal(await store1.getItem("removeTest"), null);
  Assert.equal(await store2.getItem("removeTest"), null);
});
