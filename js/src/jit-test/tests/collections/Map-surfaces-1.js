// Map surfaces

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
assertEq(Object.prototype.toString.call(new Map), "[object Map]");
assertEq(Object.prototype.toString.call(Map()), "[object Map]");
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

checkMethod("size", 0);
checkMethod("get", 1);
checkMethod("has", 1);
checkMethod("set", 2);
checkMethod("delete", 1);
