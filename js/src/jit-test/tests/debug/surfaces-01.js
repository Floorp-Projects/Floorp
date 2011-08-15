// Check superficial characteristics of functions and properties (not functionality).

function checkFunction(obj, name, nargs) {
    var desc = Object.getOwnPropertyDescriptor(obj, name);
    assertEq(desc.configurable, true, name + " should be configurable");
    assertEq(desc.writable, true, name + " should be writable");
    assertEq(desc.enumerable, false, name + " should be non-enumerable");
    assertEq(desc.value, obj[name]);  // well obviously
    assertEq(typeof desc.value, 'function', name + " should be a function");
    assertEq(desc.value.length, nargs, name + " should have .length === " + nargs);
}

checkFunction(this, "Debugger", 1);

assertEq(Debugger.prototype.constructor, Debugger);
assertEq(Object.prototype.toString.call(Debugger.prototype), "[object Debugger]");
assertEq(Object.getPrototypeOf(Debugger.prototype), Object.prototype);
