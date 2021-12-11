/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

// Array.from throws if the argument is undefined or null.
assertThrowsInstanceOf(() => Array.from(), TypeError);
assertThrowsInstanceOf(() => Array.from(undefined), TypeError);
assertThrowsInstanceOf(() => Array.from(null), TypeError);

// Array.from throws if an element can't be defined on the new object.
function ObjectWithReadOnlyElement() {
    Object.defineProperty(this, "0", {value: null});
    this.length = 0;
}
ObjectWithReadOnlyElement.from = Array.from;
assertDeepEq(ObjectWithReadOnlyElement.from([]), new ObjectWithReadOnlyElement);
assertThrowsInstanceOf(() => ObjectWithReadOnlyElement.from([1]), TypeError);

// The same, but via preventExtensions.
function InextensibleObject() {
    Object.preventExtensions(this);
}
InextensibleObject.from = Array.from;
assertThrowsInstanceOf(() => InextensibleObject.from([1]), TypeError);

// We will now test this property, that Array.from throws if the .length can't
// be assigned, using several different kinds of object.
var obj;
function init(self) {
    obj = self;
    self[0] = self[1] = self[2] = self[3] = 0;
}

function testUnsettableLength(C, Exc) {
    if (Exc === undefined)
        Exc = TypeError;  // the usual expected exception type
    C.from = Array.from;

    obj = null;
    assertThrowsInstanceOf(() => C.from([]), Exc);
    assertEq(obj instanceof C, true);
    for (var i = 0; i < 4; i++)
        assertEq(obj[0], 0);

    obj = null;
    assertThrowsInstanceOf(() => C.from([0, 10, 20, 30]), Exc);
    assertEq(obj instanceof C, true);
    for (var i = 0; i < 4; i++)
        assertEq(obj[i], i * 10);
}

// Array.from throws if the new object's .length can't be assigned because
// there is no .length and the object is inextensible.
function InextensibleObject4() {
    init(this);
    Object.preventExtensions(this);
}
testUnsettableLength(InextensibleObject4);

// Array.from throws if the new object's .length can't be assigned because it's
// read-only.
function ObjectWithReadOnlyLength() {
    init(this);
    Object.defineProperty(this, "length", {configurable: true, writable: false, value: 4});
}
testUnsettableLength(ObjectWithReadOnlyLength);

// The same, but using a builtin type.
Uint8Array.from = Array.from;
assertThrowsInstanceOf(() => Uint8Array.from([]), TypeError);

// Array.from throws if the new object's .length can't be assigned because it
// inherits a readonly .length along the prototype chain.
function ObjectWithInheritedReadOnlyLength() {
    init(this);
}
Object.defineProperty(ObjectWithInheritedReadOnlyLength.prototype,
                      "length",
                      {configurable: true, writable: false, value: 4});
testUnsettableLength(ObjectWithInheritedReadOnlyLength);

// The same, but using an object with a .length getter but no setter.
function ObjectWithGetterOnlyLength() {
    init(this);
    Object.defineProperty(this, "length", {configurable: true, get: () => 4});
}
testUnsettableLength(ObjectWithGetterOnlyLength);

// The same, but with a setter that throws.
function ObjectWithThrowingLengthSetter() {
    init(this);
    Object.defineProperty(this, "length", {
        configurable: true,
        get: () => 4,
        set: () => { throw new RangeError("surprise!"); }
    });
}
testUnsettableLength(ObjectWithThrowingLengthSetter, RangeError);

// Array.from throws if mapfn is neither callable nor undefined.
assertThrowsInstanceOf(() => Array.from([3, 4, 5], {}), TypeError);
assertThrowsInstanceOf(() => Array.from([3, 4, 5], "also not a function"), TypeError);
assertThrowsInstanceOf(() => Array.from([3, 4, 5], null), TypeError);

// Even if the function would not have been called.
assertThrowsInstanceOf(() => Array.from([], JSON), TypeError);

// If mapfn is not undefined and not callable, the error happens before anything else.
// Before calling the constructor, before touching the arrayLike.
var log = "";
function C() {
    log += "C";
    obj = this;
}
var p = new Proxy({}, {
    has: function () { log += "1"; },
    get: function () { log += "2"; },
    getOwnPropertyDescriptor: function () { log += "3"; }
});
assertThrowsInstanceOf(() => Array.from.call(C, p, {}), TypeError);
assertEq(log, "");

// If mapfn throws, the new object has already been created.
var arrayish = {
    get length() { log += "l"; return 1; },
    get 0() { log += "0"; return "q"; }
};
log = "";
var exc = {surprise: "ponies"};
assertThrowsValue(() => Array.from.call(C, arrayish, () => { throw exc; }), exc);
assertEq(log, "lC0");
assertEq(obj instanceof C, true);

// It's a TypeError if the @@iterator property is a primitive (except null and undefined).
for (var primitive of ["foo", 17, Symbol(), true]) {
    assertThrowsInstanceOf(() => Array.from({[Symbol.iterator] : primitive}), TypeError);
}
assertDeepEq(Array.from({[Symbol.iterator]: null}), []);
assertDeepEq(Array.from({[Symbol.iterator]: undefined}), []);

// It's a TypeError if the iterator's .next() method returns a primitive.
for (var primitive of [undefined, null, 17]) {
    assertThrowsInstanceOf(
        () => Array.from({
            [Symbol.iterator]() {
                return {next() { return primitive; }};
            }
        }),
        TypeError);
}

if (typeof reportCompare === 'function')
    reportCompare(0, 0);
