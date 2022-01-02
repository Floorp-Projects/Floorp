var max = 40;
setJitCompilerOption("ion.warmup.trigger", max - 10);

function defineProperty() {
    var abc = {};
    Object.defineProperty(abc, "x", {value: 1})
    assertEq(abc.x, 1);
}

function simple() {
    var o = {a: 1};
    assertEq("a" in o, true);
    assertEq("b" in o, false);
    assertEq(o.hasOwnProperty("a"), true);
    assertEq(o.hasOwnProperty("b"), false);
}

function proto() {
    var o = {a: 1, __proto__: {b: 2}};
    assertEq("a" in o, true);
    assertEq("b" in o, true);
    assertEq("c" in o, false);
    assertEq(o.hasOwnProperty("a"), true);
    assertEq(o.hasOwnProperty("b"), false);
    assertEq(o.hasOwnProperty("c"), false);
}

for (var i = 0; i < max; i++) {
    defineProperty();
    simple();
    proto();
}
