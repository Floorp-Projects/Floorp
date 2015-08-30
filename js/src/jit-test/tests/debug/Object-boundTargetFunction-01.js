// Smoke tests for bound function things.

var g = newGlobal();
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);
var arrw = gw.executeInGlobal("var arr = []; arr;").return;
var pushw = gw.executeInGlobal("var push = arr.push.bind(arr); push;").return;
assertEq(pushw.isBoundFunction, true);
assertEq(pushw.boundThis, arrw);
assertEq(pushw.boundArguments.length, 0);

var arr2 = gw.executeInGlobal("var arr2 = []; arr2").return;
assertEq(pushw.call(arr2, "tuesday").return, 1);
g.eval("assertEq(arr.length, 1);");
g.eval("assertEq(arr[0], 'tuesday');");
g.eval("assertEq(arr2.length, 0);");

g.eval("push = arr.push.bind(arr, 1, 'seven', {x: 'q'});");
pushw = gw.getOwnPropertyDescriptor("push").value;
assertEq(pushw.isBoundFunction, true);
var args = pushw.boundArguments;
assertEq(args.length, 3);
assertEq(args[0], 1);
assertEq(args[1], 'seven');
assertEq(args[2] instanceof Debugger.Object, true);
assertEq(args[2].getOwnPropertyDescriptor("x").value, "q");
