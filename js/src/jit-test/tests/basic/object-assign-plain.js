// Tests for Object.assign's fast path for plain objects.

load(libdir + "asserts.js");

function testProtoSetter() {
    var from = Object.create(null);
    from.__proto__ = {};
    assertEq(Object.getPrototypeOf(from), null);

    var to = Object.assign({}, from);
    assertEq(Object.getPrototypeOf(to), from.__proto__);
    assertEq(Object.getOwnPropertyNames(to).length, 0);
}
testProtoSetter();
testProtoSetter();
testProtoSetter();

function testProtoDataProp() {
    var to = Object.create(null);
    to.__proto__ = 1;
    var from = Object.create(null);
    from.__proto__ = 2;
    Object.assign(to, from);
    assertEq(Object.getPrototypeOf(to), null);
    assertEq(to.__proto__, 2);
}
testProtoDataProp();
testProtoDataProp();
testProtoDataProp();

function testNonExtensible() {
    var to = Object.preventExtensions({x: 1});
    Object.assign(to, {x: 2});
    assertEq(to.x, 2);
    assertThrowsInstanceOf(() => Object.assign(to, {x: 3, y: 4}), TypeError);
    assertEq(to.x, 3);
    assertEq("y" in to, false);
}
testNonExtensible();
testNonExtensible();
testNonExtensible();

function testNonExtensibleNoProps() {
    var to = Object.preventExtensions({});
    Object.assign(to, {}); // No exception.
}
testNonExtensibleNoProps();
testNonExtensibleNoProps();
testNonExtensibleNoProps();

function testDenseElements() {
    var to = Object.assign({}, {0: 1, 1: 2});
    assertEq(to[0], 1);
    assertEq(to[1], 2);
}
testDenseElements();
testDenseElements();
testDenseElements();

function testNonWritableOnProto() {
    var proto = {};
    Object.defineProperty(proto, "x", {value: 1, enumerable: true, configurable: true});
    var to = Object.create(proto);
    assertThrowsInstanceOf(() => Object.assign(to, {x: 2}), TypeError);
    assertEq(to.x, 1);
    assertEq(Object.getOwnPropertyNames(to).length, 0);
}
testNonWritableOnProto();
testNonWritableOnProto();
testNonWritableOnProto();

function testAccessorOnProto() {
    var setterVal;
    var proto = {set a(v) { setterVal = v; }};
    var to = Object.assign(Object.create(proto), {a: 9});
    assertEq(setterVal, 9);
    assertEq(Object.getOwnPropertyNames(to).length, 0);
}
testAccessorOnProto();
testAccessorOnProto();
testAccessorOnProto();

function testSetAndAdd() {
    var to = Object.assign({x: 1, y: 2}, {x: 3, y: 4, z: 5});
    assertEq(to.x, 3);
    assertEq(to.y, 4);
    assertEq(to.z, 5);
}
testSetAndAdd();
testSetAndAdd();
testSetAndAdd();

function testNonConfigurableFrom() {
    var from = {};
    Object.defineProperty(from, "x", {value: 1, enumerable: true, writable: true});
    var to = Object.assign({}, from);
    assertEq(to.x, 1);
    assertEq(Object.getOwnPropertyDescriptor(to, "x").configurable, true);
}
testNonConfigurableFrom();
testNonConfigurableFrom();
testNonConfigurableFrom();

function testNonEnumerableFrom() {
    var from = {};
    Object.defineProperty(from, "x", {value: 1, configurable: true, writable: true});
    var to = Object.assign({}, from);
    assertEq(Object.getOwnPropertyNames(to).length, 0);
    assertEq(to.x, undefined);
}
testNonEnumerableFrom();
testNonEnumerableFrom();
testNonEnumerableFrom();

function testNonWritableFrom() {
    var from = {};
    Object.defineProperty(from, "x", {value: 1, configurable: true, enumerable: true});
    var to = Object.assign({}, from);
    assertEq(to.x, 1);
    assertEq(Object.getOwnPropertyDescriptor(to, "x").writable, true);
}
testNonWritableFrom();
testNonWritableFrom();
testNonWritableFrom();

function testFrozenProto() {
    var proto = Object.freeze({x: 1});
    var target = Object.create(proto);
    Object.assign(target, {foo: 1});
    assertEq(target.foo, 1);
    assertThrowsInstanceOf(() => Object.assign(target, {x: 2}), TypeError);
    assertEq(target.x, 1);
}
testFrozenProto();
testFrozenProto();
testFrozenProto();

function testReuseShape() {
    var from = {};
    from.x = 1;
    from.y = 2;
    var to = Object.assign({}, from);
    assertEq(to.x, 1);
    assertEq(to.y, 2);
}
testReuseShape();
testReuseShape();
testReuseShape();
