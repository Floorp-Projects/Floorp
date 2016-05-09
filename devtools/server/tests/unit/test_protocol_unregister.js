const {types} = require("devtools/shared/protocol");


function run_test()
{
  types.addType("test", {
    read: (v) => { return "successful read: " + v },
    write: (v) => { return "successful write: " + v }
  });

  // Verify the type registered correctly.

  let type = types.getType("test");
  let arrayType = types.getType("array:test");
  do_check_eq(type.read("foo"), "successful read: foo");
  do_check_eq(arrayType.read(["foo"])[0], "successful read: foo");

  types.removeType("test");

  do_check_eq(type.name, "DEFUNCT:test");
  try {
    types.getType("test");
    do_check_true(false, "getType should fail");
  } catch(ex) {
    do_check_eq(ex.toString(), "Error: Unknown type: test");
  }

  try {
    type.read("foo");
    do_check_true(false, "type.read should have thrown an exception.");
  } catch(ex) {
    do_check_eq(ex.toString(), "Error: Using defunct type: test");
  }

  try {
    arrayType.read(["foo"]);
    do_check_true(false, "array:test.read should have thrown an exception.");
  } catch(ex) {
    do_check_eq(ex.toString(), "Error: Using defunct type: test");
  }

}


