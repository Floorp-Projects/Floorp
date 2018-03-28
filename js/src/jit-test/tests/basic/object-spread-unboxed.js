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

    var target = {...unboxed};
    assertDeepEq(target, {a: 0, b: true});

    target = {a: 1, c: 3, ...unboxed};
    assertDeepEq(target, {a: 0, c: 3, b: true});
}

function expando() {
    var unboxed = tryCreateUnboxedObject();
    unboxed.c = 3.5;

    var target = {...unboxed};
    assertDeepEq(target, {a: 0, b: true, c: 3.5});

    target = {a: 1, d: 3, ...unboxed};
    assertDeepEq(target, {a: 0, d: 3, b: true, c: 3.5});
}

basic();
expando();
