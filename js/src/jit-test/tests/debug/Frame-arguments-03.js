// Destructuring arguments.

var g = newGlobal('new-compartment');
var dbg = new Debugger(g);
var hits = 0;
dbg.onDebuggerStatement = function (frame) {
    var args = frame.arguments;
    assertEq(args[0], 1);
    assertEq(args.length, 4);

    assertEq(args[1] instanceof Debugger.Object, true);
    assertEq(args[1].class, "Array");
    var getprop = frame.eval("(function (p) { return this[p]; })").return;
    assertEq(getprop instanceof Debugger.Object, true);
    assertEq(getprop.apply(args[1], ["length"]).return, 2);
    assertEq(getprop.apply(args[1], [0]).return, 2);
    assertEq(getprop.apply(args[1], [1]).return, 3);

    assertEq(args[2] instanceof Debugger.Object, true);
    assertEq(args[2].class, "Object");
    var x = getprop.apply(args[2], ["x"]).return;
    assertEq(x.class, "Array"); 
    assertEq(getprop.apply(x, ["0"]).return, 4);
    assertEq(getprop.apply(args[2], ["z"]).return, 5);

    assertEq(args[3] instanceof Debugger.Object, true);
    assertEq(args[3].class, "Object");
    assertEq(getprop.apply(args[3], ["q"]).return, 6);
    hits++;
};

g.eval("function f(a, [b, c], {x: [y], z: w}, {q}) { debugger; }");
g.eval("f(1, [2, 3], {x: [4], z: 5}, {q: 6});");
assertEq(hits, 1);
