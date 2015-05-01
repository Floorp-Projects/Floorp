// Set surfaces

load(libdir + "iteration.js");

var desc = Object.getOwnPropertyDescriptor(this, "Set");
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);
assertEq(desc.writable, true);

assertEq(typeof Set, 'function');
assertEq(Object.keys(Set).length, 0);
assertEq(Set.length, 0);
assertEq(Set.name, "Set");

assertEq(Object.getPrototypeOf(Set.prototype), Object.prototype);
assertEq(Object.prototype.toString.call(Set.prototype), "[object Set]");
assertEq(Object.prototype.toString.call(new Set()), "[object Set]");
assertEq(Object.keys(Set.prototype).join(), "");
assertEq(Set.prototype.constructor, Set);

function checkMethod(name, arity) { 
    var desc = Object.getOwnPropertyDescriptor(Set.prototype, name);
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
checkMethod("values", 0);
checkMethod("entries", 0);

var desc = Object.getOwnPropertyDescriptor(Set.prototype, "size");
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);
assertEq(typeof desc.get, 'function');
assertEq(desc.get.length, 0);
assertEq(desc.set, undefined);
checkMethod("clear", 0);

// Set.prototype.keys, .values, and .iterator are the same function object
assertEq(Set.prototype.keys, Set.prototype.values);
assertEq(Set.prototype[Symbol.iterator], Set.prototype.values);
