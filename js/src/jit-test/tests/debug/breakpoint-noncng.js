// Breakpoints work in non-compile-and-go code. Bug 738479.

var g = newGlobal('new-compartment');
g.s = '';
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);
g.evaluate(
    "function f() {\n" +  // fscript.startLine
    "    s += 'a';\n" +   // fscript.startLine + 1
    "    s += 'b';\n" +   // fscript.startLine + 2
    "}\n",
    {compileAndGo: false});

var fscript = gw.makeDebuggeeValue(g.f).script;
var handler = {hit: function (frame) { g.s += '1'; }};
for (var pc of fscript.getLineOffsets(fscript.startLine + 2))
    fscript.setBreakpoint(pc, handler);

g.f();

assertEq(g.s, "a1b");
