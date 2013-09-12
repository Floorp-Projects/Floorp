// After returning from an implicit toString call, the calling frame's onStep
// hook fires.

var g = newGlobal();
g.eval("var originalX = {toString: function () { debugger; log += 'x'; return 1; }};\n");

var dbg = Debugger(g);
dbg.onDebuggerStatement = function (frame) {
    g.log += 'd';
    frame.older.onStep = function () {
        if (!g.log.match(/[sy]$/))
            g.log += 's';
    };
};

// expr is an expression that will trigger an implicit toString call.
function check(expr) {
    g.log = '';
    g.x = g.originalX;
    g.eval(expr + ";\n" +
	   "log += 'y';\n");
    assertEq(g.log, 'dxsy');
}

check("'' + x");
check("0 + x");
check("0 - x");
check("0 * x");
check("0 / x");
check("0 % x");
check("+x");
check("x in {}");
check("x++");
check("++x");
check("x--");
check("--x");
check("x < 0");
check("x > 0");
check("x >= 0");
check("x <= 0");
check("x == 0");
check("x != 0");
check("x & 1");
check("x | 1");
check("x ^ 1");
check("~x");
check("x << 1");
check("x >> 1");
check("x >>> 1");

g.eval("function lastStep() { throw StopIteration; }");
g.eval("function emptyIterator() { debugger; log += 'x'; return { next: lastStep }; }");
g.eval("var customEmptyIterator = { __iterator__: emptyIterator };");
g.log = '';
g.eval("for (i in customEmptyIterator);\n" +
       "log += 'y';\n");
assertEq(g.log, 'dxsy');

g.eval("var getter = { get x() { debugger; return log += 'x'; } }");
check("getter.x");

g.eval("var setter = { set x(v) { debugger; return log += 'x'; } }");
check("setter.x = 1");

g.eval("Object.defineProperty(this, 'thisgetter', { get: function() { debugger; log += 'x'; }});");
check("thisgetter");
