// The tracejit does not interfere with frame.onStep.
//
// The function f() writes 'L' to the log in a loop. If we enable stepping and
// write an 's' each time frame.onStep is called, any two Ls should have at
// least one 's' between them.

var g = newGlobal();
g.N = 11;
g.log = '';
g.eval("function f() {\n" +
       "    for (var i = 0; i <= N; i++)\n" +
       "        log += 'L';\n" +
       "}\n");
g.f();
assertEq(/LL/.exec(g.log) !== null, true);

var dbg = Debugger(g);
dbg.onEnterFrame = function (frame) {
    frame.onStep = function () { g.log += 's'; };
};
g.log = '';
g.f();
assertEq(/LL/.exec(g.log), null);
