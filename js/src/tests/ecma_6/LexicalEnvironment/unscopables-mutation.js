// When obj[@@unscopables].x changes, bindings appear and disappear accordingly.

let x = "global";
function getX() { return x; }

let unscopables = {x: true};
let obj = {x: "obj", [Symbol.unscopables]: unscopables};

with (obj) {
    assertEq(x, "global");
    x = "global-1";
    assertEq(x, "global-1");
    assertEq(obj.x, "obj");

    unscopables.x = false;  // suddenly x appears in the with-environment

    assertEq(x, "obj");
    x = "obj-1";
    assertEq(getX(), "global-1");  // unchanged
    assertEq(obj.x, "obj-1");

    unscopables.x = true;  // *poof*

    assertEq(x, "global-1");
    x = "global-2";
    assertEq(getX(), "global-2");
    assertEq(obj.x, "obj-1");  // unchanged

    // The determination of which binding is assigned happens when the LHS of
    // assignment is evaluated, before the RHS. This is observable if we make
    // the binding appear or disappear during evaluation of the RHS, before
    // assigning.
    x = (unscopables.x = false, "global-3");
    assertEq(getX(), "global-3");
    assertEq(obj.x, "obj-1");

    x = (unscopables.x = true, "obj-2");
    assertEq(getX(), "global-3");
    assertEq(obj.x, "obj-2");
}

assertEq(x, "global-3");

reportCompare(0, 0);
