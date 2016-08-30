// WeakMap surfaces

var desc = Object.getOwnPropertyDescriptor(this, "WeakMap");
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);
assertEq(desc.writable, true);

assertEq(typeof WeakMap, 'function');
assertEq(Object.keys(WeakMap).length, 0);
assertEq(WeakMap.length, 0);
assertEq(WeakMap.name, "WeakMap");

assertEq(Object.getPrototypeOf(WeakMap.prototype), Object.prototype);
assertEq(Object.prototype.toString.call(WeakMap.prototype), "[object WeakMap]");
assertEq(Object.prototype.toString.call(new WeakMap()), "[object WeakMap]");
assertEq(Object.keys(WeakMap.prototype).join(), "");
assertEq(WeakMap.prototype.constructor, WeakMap);

function checkMethod(name, arity) {
    var desc = Object.getOwnPropertyDescriptor(WeakMap.prototype, name);
    assertEq(desc.enumerable, false);
    assertEq(desc.configurable, true);
    assertEq(desc.writable, true);
    assertEq(typeof desc.value, 'function');
    assertEq(desc.value.name, name);
    assertEq(desc.value.length, arity);
}

checkMethod("get", 1);
checkMethod("has", 1);
checkMethod("set", 2);
checkMethod("delete", 1);
