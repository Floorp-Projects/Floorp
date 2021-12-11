// frame properties throw if !frame.onStack

load(libdir + "asserts.js");

var g = newGlobal({newCompartment: true});
var f;
Debugger(g).onDebuggerStatement = function (frame) {
    assertEq(frame.onStack, true);
    assertEq(frame.type, "call");
    assertEq(frame.this instanceof Object, true);
    assertEq(frame.older instanceof Debugger.Frame, true);
    assertEq(frame.callee instanceof Debugger.Object, true);
    assertEq(frame.constructing, false);
    assertEq(frame.arguments.length, 0);
    f = frame;
};

g.eval("(function () { debugger; }).call({});");
assertEq(f.onStack, false);
assertThrowsInstanceOf(function () { f.type; }, Error);
assertThrowsInstanceOf(function () { f.this; }, Error);
assertThrowsInstanceOf(function () { f.older; }, Error);
assertThrowsInstanceOf(function () { f.callee; }, Error);
assertThrowsInstanceOf(function () { f.constructing; }, Error);
assertThrowsInstanceOf(function () { f.arguments; }, Error);
