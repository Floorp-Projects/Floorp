// After clearing one breakpoint, another breakpoint at the same instruction still works.

var g = newGlobal('new-compartment');
var dbg = Debugger(g);
var script = null;
var handlers = [];
dbg.onDebuggerStatement = function (frame) {
    function handler(i) {
        return {hit: function (frame) { g.log += '' + i; }};
    }
    var f = frame.eval("f").return;
    var s = f.script;
    if (script === null)
        script = s;
    else
        assertEq(s, script);

    var offs = s.getLineOffsets(g.line0 + 3);
    for (var i = 0; i < 3; i++) {
        handlers[i] = handler(i);
        for (var j = 0; j < offs.length; j++)
            s.setBreakpoint(offs[j], handlers[i]);
    }
};

g.eval("var line0 = Error().lineNumber;\n" +
       "function f() {\n" +     // line0 + 1
       "    log += 'x';\n" +    // line0 + 2
       "    log += 'y';\n" +    // line0 + 3
       "}\n" +
       "debugger;\n");
assertEq(handlers.length, 3);

g.log = '';
g.f();
assertEq(g.log, 'x012y');

script.clearBreakpoint(handlers[0]);

g.log = '';
g.f();
assertEq(g.log, 'x12y');
