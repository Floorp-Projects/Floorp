// Map surfaces

load(libdir + "iteration.js");

var desc = Object.getOwnPropertyDescriptor(this, "Map");
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);
assertEq(desc.writable, true);

assertEq(typeof Map, 'function');
assertEq(Object.keys(Map).length, 0);
assertEq(Map.length, 1);
assertEq(Map.name, "Map");

assertEq(Object.getPrototypeOf(Map.prototype), Object.prototype);
assertEq(Object.prototype.toString.call(Map.prototype), "[object Map]");
assertEq(Object.prototype.toString.call(new Map()), "[object Map]");
assertEq(Object.keys(Map.prototype).join(), "");
assertEq(Map.prototype.constructor, Map);

function checkMethod(name, arity) { 
    var desc = Object.getOwnPropertyDescriptor(Map.prototype, name);
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
checkMethod("keys", 0);
checkMethod("values", 0);
checkMethod("entries", 0);

var desc = Object.getOwnPropertyDescriptor(Map.prototype, "size");
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);
assertEq(typeof desc.get, 'function');
assertEq(desc.get.length, 0);
assertEq(desc.set, undefined);
checkMethod("clear", 0);

// Map.prototype[@@iterator] and .entries are the same function object.
assertEq(Map.prototype[Symbol.iterator], Map.prototype.entries);
