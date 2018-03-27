load(libdir + "asserts.js");

function Unboxed() {
    this.a = 0;
    this.b = true;
}

function tryCreateUnboxedObject() {
    var obj;
    for (var i = 0; i < 1000; ++i) {
        obj = new Unboxed();
    }
    if (unboxedObjectsEnabled())
        assertEq(isUnboxedObject(obj), true);
    return obj;
}

function basic() {
    var unboxed = tryCreateUnboxedObject();

    var {...target} = unboxed;
    assertDeepEq(target, {a: 0, b: true});

    var {a, c, ...target} = unboxed;
    assertDeepEq(a, 0);
    assertDeepEq(c, undefined);
    assertDeepEq(target, {b: true});
}

function expando() {
    var unboxed = tryCreateUnboxedObject();
    unboxed.c = 3.5;

    var {...target} = unboxed;
    assertDeepEq(target, {a: 0, b: true, c: 3.5});

    var {a, d, ...target} = unboxed;
    assertDeepEq(a, 0);
    assertDeepEq(d, undefined);
    assertDeepEq(target, {b: true, c: 3.5});
}

basic();
expando();
