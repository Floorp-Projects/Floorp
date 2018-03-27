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

    var target = {};
    Object.assign(target, unboxed);
    assertDeepEq(target, {a: 0, b: true});

    target = {a: 1, c: 3};
    Object.assign(target, unboxed);
    assertDeepEq(target, {a: 0, c: 3, b: true});
}

function expando() {
    var unboxed = tryCreateUnboxedObject();
    unboxed.c = 3.5;

    var target = {};
    Object.assign(target, unboxed);
    assertDeepEq(target, {a: 0, b: true, c: 3.5});

    target = {a: 1, d: 3};
    Object.assign(target, unboxed);
    assertDeepEq(target, {a: 0, d: 3, b: true, c: 3.5});
}

function addExpando() {
    var unboxed = tryCreateUnboxedObject();

    function setA(value) {
        assertEq(value, 0);
        unboxed.c = 3.5;
    }

    var target = {};
    Object.defineProperty(target, "a", {set: setA});

    var reference = {};
    Object.defineProperty(reference, "a", {set: setA});
    Object.defineProperty(reference, "b", {value: true, enumerable: true, configurable: true, writable: true});

    Object.assign(target, unboxed);
    assertDeepEq(target, reference);
}

function makeNative() {
    var unboxed = tryCreateUnboxedObject();

    function setA(value) {
        assertEq(value, 0);
        Object.defineProperty(unboxed, "a", {writable: false});
    }

    var target = {};
    Object.defineProperty(target, "a", {set: setA});

    var reference = {};
    Object.defineProperty(reference, "a", {set: setA});
    Object.defineProperty(reference, "b", {value: true, enumerable: true, configurable: true, writable: true});

    Object.assign(target, unboxed);
    assertDeepEq(target, reference);
}


basic();
expando();
addExpando();
makeNative();
