load(libdir + "asserts.js");

function testMethod(name, hasArg = true) {
    var method = WeakSet.prototype[name];

    assertThrowsInstanceOf(function() { method.call(1); }, TypeError);
    assertThrowsInstanceOf(function() { method.call({}); }, TypeError);
    assertThrowsInstanceOf(function() { method.call(new WeakMap); }, TypeError);
    assertThrowsInstanceOf(function() { method.call(WeakSet.prototype); }, TypeError);

    if (hasArg) {
        assertThrowsInstanceOf(function() { method.call(new WeakSet, 1); }, TypeError);
        assertThrowsInstanceOf(function() { method.call(WeakSet.prototype, 1); }, TypeError);
    }
}

testMethod("has");
testMethod("add");
testMethod("delete");
testMethod("clear", false);

assertThrowsInstanceOf(function() { new WeakSet({[Symbol.iterator]: 2}) }, TypeError);
assertEq(typeof [][Symbol.iterator], "function");

assertThrowsInstanceOf(function() { WeakSet(); }, TypeError);
