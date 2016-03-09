var g = newGlobal();
var dbg = Debugger(g);
dbg.onDebuggerStatement = function (frame) {
    try {
        frame.arguments[0].deleteProperty("x");
    } catch (exc) {
        assertEq(exc instanceof Debugger.DebuggeeWouldRun, true);
        return;
    }
    throw new Error("deleteProperty should throw");
};

g.evaluate("function h(obj) { debugger; } \n" +
           "h(new Proxy({}, \n" +
           "            { deleteProperty: function () { \n" +
           "                var e = new ReferenceError('diaf', 'fail'); \n" +
           "                throw e; \n" +
           "              } \n" +
           "            }));");
