// Test that inspecting a bound function that was bound again does the right
// thing.

var g = newGlobal();
var dbg = new Debugger();
var gw = dbg.addDebuggee(g);
var expr = "function f() { return this; }; var bf = f.bind(1, 2).bind(3, 4); bf";
var bfw = gw.executeInGlobal(expr).return;

assertEq(bfw.isBoundFunction, true);
assertEq(bfw.boundThis, 3);
var args = bfw.boundArguments;
assertEq(args.length, 1);
assertEq(args[0], 4);

assertEq(bfw.boundTargetFunction.isBoundFunction, true);
assertEq(bfw.boundTargetFunction.boundThis, 1);
args = bfw.boundTargetFunction.boundArguments;
assertEq(args.length, 1);
assertEq(args[0], 2);
