/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function checkDataProperty(object, propertyKey, value, writable, enumerable, configurable) {
    var desc = Object.getOwnPropertyDescriptor(object, propertyKey);
    assertEq(desc === undefined, false);
    assertEq('value' in desc, true);
    assertEq(desc.value, value);
    assertEq(desc.writable, writable);
    assertEq(desc.enumerable, enumerable);
    assertEq(desc.configurable, configurable);
}

/* 19.1.2.1  Object.assign ( target, ...sources ) */
assertEq(Object.assign.length, 2);

// Basic functionality works with multiple sources
function basicMultipleSources() {
    var a = {};
    var b = { bProp : 7 };
    var c = { cProp : 8 };
    Object.assign(a, b, c);
    assertEq(a.bProp, 7);
    assertEq(a.cProp, 8);
}
basicMultipleSources();

// Basic functionality works with symbols (Bug 1052358)
function basicSymbols() {
    var a = {};
    var b = { bProp : 7 };
    var aSymbol = Symbol("aSymbol");
    b[aSymbol] = 22;
    Object.assign(a, b);
    assertEq(a.bProp, 7);
    assertEq(a[aSymbol], 22);
}
basicSymbols();

// Calls ToObject() for target, skips null/undefined sources, uses
// ToObject(source) otherwise.
function testToObject() {
    assertThrowsInstanceOf(() => Object.assign(null, null), TypeError);
    assertThrowsInstanceOf(() => Object.assign(), TypeError);
    assertThrowsInstanceOf(() => Object.assign(null, {}), TypeError);
    assertEq(Object.assign({}, null) instanceof Object, true);
    assertEq(Object.assign({}, undefined) instanceof Object, true);

    // Technically an embedding could have this as extension acting differently
    // from ours, so a feature-test is inadequate.  We can move this subtest
    // into extensions/ if that ever matters.
    if (typeof objectEmulatingUndefined === "function") {
        var falsyObject = objectEmulatingUndefined();
        falsyObject.foo = 7;

        var obj = Object.assign({}, falsyObject);
        assertEq(obj instanceof Object, true);
        assertEq(obj.foo, 7);
    }

    assertEq(Object.assign(true, {}) instanceof Boolean, true);
    assertEq(Object.assign(1, {}) instanceof Number, true);
    assertEq(Object.assign("string", {}) instanceof String, true);
    var o = {};
    assertEq(Object.assign(o, {}), o);
}
testToObject();

// Invokes [[OwnPropertyKeys]] on ToObject(source)
function testOwnPropertyKeys() {
    assertThrowsInstanceOf(() => Object.assign(null, new Proxy({}, {
        getOwnPropertyNames: () => { throw new Error("not called"); }
    })), TypeError);

    var ownKeysCalled = false;
    Object.assign({}, new Proxy({}, {
        ownKeys: function() {
            ownKeysCalled = true;
            return [];
        }
    }));
    assertEq(ownKeysCalled, true);
};
testOwnPropertyKeys();

// Ensure correct property traversal
function correctPropertyTraversal() {
    var log = "";
    var source = new Proxy({a: 1, b: 2}, {
        ownKeys: () => ["b", "c", "a"],
        getOwnPropertyDescriptor: function(t, pk) {
            log += "#" + pk;
            return Object.getOwnPropertyDescriptor(t, pk);
        },
        get: function(t, pk, r) {
            log += "-" + pk;
            return t[pk];
        },
    });
    Object.assign({}, source);
    assertEq(log, "#b-b#c#a-a");
}
correctPropertyTraversal();

// Only [[Enumerable]] properties are assigned to target
function onlyEnumerablePropertiesAssigned() {
    var source = Object.defineProperties({}, {
        a: {value: 1, enumerable: true},
        b: {value: 2, enumerable: false},
    });
    var target = Object.assign({}, source);
    assertEq("a" in target, true);
    assertEq("b" in target, false);
}
onlyEnumerablePropertiesAssigned();


// Enumerability is decided on-time, not before main loop (1)
function testEnumerabilityDeterminedInLoop1()
{
    var getterCalled = false;
    var sourceTarget = {
        get a() { getterCalled = true },
        get b() { Object.defineProperty(sourceTarget, "a", {enumerable: false}) },
    };
    var source = new Proxy(sourceTarget, { ownKeys: () => ["b", "a"] });
    Object.assign({}, source);
    assertEq(getterCalled, false);
}
testEnumerabilityDeterminedInLoop1();

// Enumerability is decided on-time, not before main loop (2)
function testEnumerabilityDeterminedInLoop2()
{
    var getterCalled = false;
    var sourceTarget = {
        get a() { getterCalled = true },
        get b() { Object.defineProperty(sourceTarget, "a", {enumerable: true}) },
    };
    var source = new Proxy(sourceTarget, {
        ownKeys: () => ["b", "a"]
    });
    Object.defineProperty(sourceTarget, "a", {enumerable: false});
    Object.assign({}, source);
    assertEq(getterCalled, true);
}
testEnumerabilityDeterminedInLoop2();

// Properties are retrieved through Get() and assigned onto
// the target as data properties, not in any sense cloned over as descriptors
function testPropertiesRetrievedThroughGet() {
    var getterCalled = false;
    Object.assign({}, {get a() { getterCalled = true }});
    assertEq(getterCalled, true);
}
testPropertiesRetrievedThroughGet();

// Properties are retrieved through Get()
// Properties are assigned through Put()
function testPropertiesAssignedThroughPut() {
    var setterCalled = false;
    Object.assign({set a(v) { setterCalled = v }}, {a: true});
    assertEq(setterCalled, true);
}
testPropertiesAssignedThroughPut();

// Properties are retrieved through Get()
// Properties are assigned through Put(): Existing property attributes are not altered
function propertiesAssignedExistingNotAltered() {
    var source = {a: 1, b: 2, c: 3};
    var target = {a: 0, b: 0, c: 0};
    Object.defineProperty(target, "a", {enumerable: false});
    Object.defineProperty(target, "b", {configurable: false});
    Object.defineProperty(target, "c", {enumerable: false, configurable: false});
    Object.assign(target, source);
    checkDataProperty(target, "a", 1, true, false, true);
    checkDataProperty(target, "b", 2, true, true, false);
    checkDataProperty(target, "c", 3, true, false, false);
}
propertiesAssignedExistingNotAltered();

// Properties are retrieved through Get()
// Properties are assigned through Put(): Throws TypeError if non-writable
function propertiesAssignedTypeErrorNonWritable() {
    var source = {a: 1};
    var target = {a: 0};
    Object.defineProperty(target, "a", {writable: false});
    assertThrowsInstanceOf(() => Object.assign(target, source), TypeError);
    checkDataProperty(target, "a", 0, false, true, true);
}
propertiesAssignedTypeErrorNonWritable();

// Properties are retrieved through Get()
// Put() creates standard properties; Property attributes from source are ignored
function createsStandardProperties() {
    var source = {a: 1, b: 2, c: 3, get d() { return 4 }};
    Object.defineProperty(source, "b", {writable: false});
    Object.defineProperty(source, "c", {configurable: false});
    var target = Object.assign({}, source);
    checkDataProperty(target, "a", 1, true, true, true);
    checkDataProperty(target, "b", 2, true, true, true);
    checkDataProperty(target, "c", 3, true, true, true);
    checkDataProperty(target, "d", 4, true, true, true);
}
createsStandardProperties();

// Properties created during traversal are not copied
function propertiesCreatedDuringTraversalNotCopied() {
    var source = {get a() { this.b = 2 }};
    var target = Object.assign({}, source);
    assertEq("a" in target, true);
    assertEq("b" in target, false);
}
propertiesCreatedDuringTraversalNotCopied();

// Properties deleted during traversal are not copied
function testDeletePropertiesNotCopied() {
    var source = new Proxy({
        get a() { delete this.b },
        b: 2,
    }, {
        getOwnPropertyNames: () => ["a", "b"]
    });
    var target = Object.assign({}, source);
    assertEq("a" in target, true);
    assertEq("b" in target, false);
}
testDeletePropertiesNotCopied();

function testDeletionExposingShadowedProperty()
{
    var srcProto = { b: 42 };
    var src =
        Object.create(srcProto,
                        { a: { enumerable: true, get: function() { delete this.b; } },
                          b: { value: 2, configurable: true, enumerable: true } });
    var source = new Proxy(src, { getOwnPropertyNames: () => ["a", "b"] });
    var target = Object.assign({}, source);
    assertEq("a" in target, true);
    assertEq("b" in target, false);
}
testDeletionExposingShadowedProperty();

// Properties first deleted and then recreated during traversal are copied (1)
function testDeletedAndRecreatedPropertiesCopied1() {
    var source = new Proxy({
        get a() { delete this.c },
        get b() { this.c = 4 },
        c: 3,
    }, {
        getOwnPropertyNames: () => ["a", "b", "c"]
    });
    var target = Object.assign({}, source);
    assertEq("a" in target, true);
    assertEq("b" in target, true);
    assertEq("c" in target, true);
    checkDataProperty(target, "c", 4, true, true, true);
}
testDeletedAndRecreatedPropertiesCopied1();

// Properties first deleted and then recreated during traversal are copied (2)
function testDeletedAndRecreatedPropertiesCopied2() {
    var source = new Proxy({
        get a() { delete this.c },
        get b() { this.c = 4 },
        c: 3,
    }, {
        ownKeys: () => ["a", "c", "b"]
    });
    var target = Object.assign({}, source);
    assertEq("a" in target, true);
    assertEq("b" in target, true);
    assertEq("c" in target, false);
}
testDeletedAndRecreatedPropertiesCopied2();

// String and Symbol valued properties are copied
function testStringAndSymbolPropertiesCopied() {
    var keyA = "str-prop";
    var source = {"str-prop": 1};
    var target = Object.assign({}, source);
    checkDataProperty(target, keyA, 1, true, true, true);
}
testStringAndSymbolPropertiesCopied();

// Intermediate exceptions do not stop property traversal, first exception is reported (1)
function testExceptionsDoNotStopFirstReported1() {
    var ErrorA = function ErrorA() {};
    var ErrorB = function ErrorB() {};
    var log = "";
    var source = new Proxy({}, {
        getOwnPropertyDescriptor: function(t, pk) {
            log += pk;
            throw new (pk === "a" ? ErrorA : ErrorB);
        },
        ownKeys: () => ["b", "a"]
    });
    assertThrowsInstanceOf(() => Object.assign({}, source), ErrorB);
    assertEq(log, "ba");
}
testExceptionsDoNotStopFirstReported1();

// Properties are retrieved through Get()
// Intermediate exceptions do not stop property traversal, first exception is reported (2)
function testExceptionsDoNotStopFirstReported2() {
    var ErrorA = function ErrorA() {};
    var ErrorB = function ErrorB() {};
    var log = "";
    var source = new Proxy({
        get a() { log += "a"; throw new ErrorA },
        get b() { log += "b"; throw new ErrorB },
    }, {
        ownKeys: () => ["b", "a"]
    });
    assertThrowsInstanceOf(() => Object.assign({}, source), ErrorB);
    assertEq(log, "ba");
}
testExceptionsDoNotStopFirstReported2();

// Intermediate exceptions do not stop property traversal, first exception is reported (3)
function testExceptionsDoNotStopFirstReported3() {
    var ErrorA = function ErrorA() {};
    var ErrorB = function ErrorB() {};
    var log = "";
    var source = new Proxy({a: 1, b: 2}, {
        ownKeys: () => ["b", "a"]
    });
    var target = {
        set a(v) { log += "a"; throw new ErrorA },
        set b(v) { log += "b"; throw new ErrorB },
    };
    assertThrowsInstanceOf(() => Object.assign(target, source), ErrorB);
    assertEq(log, "ba");
}
testExceptionsDoNotStopFirstReported3();

// Intermediate exceptions do not stop property traversal, first exception is reported (4)
function testExceptionsDoNotStopFirstReported4() {
    var ErrorGetOwnProperty = function ErrorGetOwnProperty() {};
    var ErrorGet = function ErrorGet() {};
    var ErrorSet = function ErrorSet() {};
    var source = new Proxy({
        get a() { throw new ErrorGet }
    }, {
        getOwnPropertyDescriptor: function(t, pk) {
            throw new ErrorGetOwnProperty;
        }
    });
    var target = {
        set a(v) { throw new ErrorSet }
    };
    assertThrowsInstanceOf(() => Object.assign({}, source), ErrorGetOwnProperty);
}
testExceptionsDoNotStopFirstReported4();

if (typeof reportCompare == "function")
    reportCompare(true, true);
