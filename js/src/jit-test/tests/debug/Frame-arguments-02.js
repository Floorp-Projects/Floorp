// Object arguments.

var g = newGlobal('new-compartment');
var dbg = new Debugger(g);
var hits = 0;
dbg.onDebuggerStatement = function (frame) {
    var args = frame.arguments;
    assertEq(args, frame.arguments);
    assertEq(args instanceof Array, true);
    assertEq(args.length, 2);
    assertEq(args[0] instanceof Debugger.Object, true);
    assertEq(args[0].class, args[1]);
    hits++;
};

g.eval("function f(obj, cls) { debugger; }");
g.eval("f({}, 'Object');");
g.eval("f(Date, 'Function');");
assertEq(hits, 2);
