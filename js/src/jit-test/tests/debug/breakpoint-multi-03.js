// Multiple Debugger objects can set breakpoints at the same instruction.

var g = newGlobal('new-compartment');
function attach(g, i) {
    var dbg = Debugger(g);
    dbg.onDebuggerStatement = function (frame) {
        var s = frame.eval("f").return.script;
        var offs = s.getLineOffsets(g.line0 + 3);
        for (var j = 0; j < offs.length; j++)
            s.setBreakpoint(offs[j], {hit: function () { g.log += "" + i; }});
    };
}

g.eval("var line0 = Error().lineNumber;\n" +
       "function f() {\n" +     // line0 + 1
       "    log += 'a';\n" +    // line0 + 2
       "    log += 'b';\n" +    // line0 + 3
       "}\n");

for (var i = 0; i < 3; i++)
    attach(g, i);

g.log = '';
g.eval('debugger;');
g.log += 'x';
g.f();
assertEq(g.log, 'xa012b');
