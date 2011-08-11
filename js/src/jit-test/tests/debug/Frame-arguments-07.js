// When argument[x] is assigned, where x > callee.length, frame.arguments reflects the change.

var g = newGlobal('new-compartment');
g.eval("function f(a, b) {\n" +
       "    for (var i = 0; i < arguments.length; i++)\n" +
       "        arguments[i] = i;\n" +
       "    debugger;\n" +
       "}\n");

var dbg = Debugger(g);
var hits = 0;
dbg.onDebuggerStatement = function (frame) {
    var argc = frame.eval("arguments.length").return;
    var args = frame.arguments;
    assertEq(args.length, argc);
    for (var i = 0; i < argc; i++)
	assertEq(args[i], i);
    hits++;
}

g.f(9);
g.f(9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9);
assertEq(hits, 2);
