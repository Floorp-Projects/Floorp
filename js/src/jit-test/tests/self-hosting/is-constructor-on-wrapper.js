var g = newGlobal();
var w = g.eval("() => {}");
var v = g.eval("Array");

try {
    Reflect.construct(Array, [], w);
    assertEq(true, false, "Expected exception above");
} catch (e) {
    assertEq(e.constructor, TypeError);
}

try {
    Reflect.construct(v, [], w);
    assertEq(true, false, "Expected exception above");
} catch (e) {
    assertEq(e.constructor, TypeError);
}
