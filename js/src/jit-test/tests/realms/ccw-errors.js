function test() {
  "use strict";

  const g = newGlobal({newCompartment: true});
  Error.prototype.whence = "main global";
  g.eval("Error.prototype.whence = 'other global'");

  const obj = g.eval("[]");
  Object.freeze(obj);
  try {
    obj.foo = 7;
    assertEq("reached", "no", "This line should not be reached; the previous line should have thrown");
  } catch(e) {
    assertEq("" + e, `TypeError: can't define property "foo": Array is not extensible`);
    assertEq(e.whence, "main global"); // setting operation happens in this global
  }

  const obj2 = g.eval(`obj2 = { get x() { throw new Error("go away"); } };`);
  try {
    obj2.x;
    assertEq("reached", "no", "This line should not be reached; the previous line should have thrown");
  } catch(e) {
    assertEq("" + e, `Error: go away`);
    assertEq(e.whence, "other global"); // Error created in other global
  }
}

test();
