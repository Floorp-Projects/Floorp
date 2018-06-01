"use strict";

const {types} = require("devtools/shared/protocol");

function run_test() {
  types.addType("test", {
    read: (v) => "successful read: " + v,
    write: (v) => "successful write: " + v
  });

  // Verify the type registered correctly.

  const type = types.getType("test");
  const arrayType = types.getType("array:test");
  Assert.equal(type.read("foo"), "successful read: foo");
  Assert.equal(arrayType.read(["foo"])[0], "successful read: foo");

  types.removeType("test");

  Assert.equal(type.name, "DEFUNCT:test");
  try {
    types.getType("test");
    Assert.ok(false, "getType should fail");
  } catch (ex) {
    Assert.equal(ex.toString(), "Error: Unknown type: test");
  }

  try {
    type.read("foo");
    Assert.ok(false, "type.read should have thrown an exception.");
  } catch (ex) {
    Assert.equal(ex.toString(), "Error: Using defunct type: test");
  }

  try {
    arrayType.read(["foo"]);
    Assert.ok(false, "array:test.read should have thrown an exception.");
  } catch (ex) {
    Assert.equal(ex.toString(), "Error: Using defunct type: test");
  }
}

