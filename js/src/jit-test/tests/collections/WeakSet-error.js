load(libdir + "asserts.js");
load(libdir + "iteration.js");

function testMethod(name) {
    var method = WeakSet.prototype[name];

    assertThrowsInstanceOf(function() { method.call(1); }, TypeError);
    assertThrowsInstanceOf(function() { method.call({}); }, TypeError);
    assertThrowsInstanceOf(function() { method.call(new WeakMap); }, TypeError);
    assertThrowsInstanceOf(function() { method.call(WeakSet.prototype); }, TypeError);
}

testMethod("has");
testMethod("add");
testMethod("delete");
testMethod("clear");

assertThrowsInstanceOf(function() { var ws = new WeakSet(); ws.add(1); }, TypeError);
assertThrowsInstanceOf(function() { new WeakSet({[std_iterator]: 2}) }, TypeError);
assertEq(typeof [][std_iterator], "function");

assertThrowsInstanceOf(function() { WeakSet(); }, TypeError);
