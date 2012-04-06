// setVariable can create a new property on a with block's bindings object, if
// it is shadowing an existing property on the prototype chain.

var g = newGlobal('new-compartment');
var dbg = Debugger(g);
dbg.onDebuggerStatement = function (frame) {
    var env = frame.environment.find("x");
    env.setVariable("x", 2);
};
g.eval("var obj1 = {x: 1}, obj2 = Object.create(obj1), z; with (obj2) { debugger; z = x; }");
assertEq(g.obj1.x, 1);
assertEq(g.obj2.x, 2);
assertEq(g.z, 2);

// The property created by setVariable is like the one created by ordinary
// assignment in a with-block.
var desc = Object.getOwnPropertyDescriptor(g.obj2, "x");
assertEq(desc.configurable, true);
assertEq(desc.enumerable, true);
assertEq(desc.writable, true);
assertEq(desc.value, 2);
