// WeakSet surfaces

var desc = Object.getOwnPropertyDescriptor(this, "WeakSet");
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);
assertEq(desc.writable, true);

assertEq(typeof WeakSet, 'function');
assertEq(Object.keys(WeakSet).length, 0);
assertEq(WeakSet.length, 0);
assertEq(WeakSet.name, "WeakSet");

assertEq(Object.getPrototypeOf(WeakSet.prototype), Object.prototype);
assertEq(Object.prototype.toString.call(WeakSet.prototype), "[object WeakSet]");
assertEq(Object.prototype.toString.call(new WeakSet), "[object WeakSet]");
assertEq(Object.keys(WeakSet.prototype).length, 0);
assertEq(WeakSet.prototype.constructor, WeakSet);

function checkMethod(name, arity) {
    var desc = Object.getOwnPropertyDescriptor(WeakSet.prototype, name);
    assertEq(desc.enumerable, false);
    assertEq(desc.configurable, true);
    assertEq(desc.writable, true);
    assertEq(typeof desc.value, 'function');
    assertEq(desc.value.name, name);
    assertEq(desc.value.length, arity);
}

checkMethod("has", 1);
checkMethod("add", 1);
checkMethod("delete", 1);
checkMethod("clear", 0);
