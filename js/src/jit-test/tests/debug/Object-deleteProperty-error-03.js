var g = newGlobal();
var dbg = Debugger(g);
dbg.onDebuggerStatement = function (frame) {
    try {
        frame.arguments[0].deleteProperty("x");
    } catch (exc) {
        assertEq(exc instanceof ReferenceError, true);

        // These aren't technically what should be expected, of course.  But
        // this is what ErrorCopier::~ErrorCopier does right now, so roll with
        // it.
        assertEq(exc.fileName, "");
        assertEq(exc.lineNumber, 0);
        assertEq(exc.columnNumber, 0);
        assertEq(exc.stack, "");
        return;
    }
    throw new Error("deleteProperty should throw");
};

g.evaluate("function h(obj) { debugger; } \n" +
           "h(new Proxy({}, \n" +
           "            { deleteProperty: function () { \n" +
           "                var e = new ReferenceError('diaf', 'fail'); \n" +
           "                e.fileName = e; \n" +
           "                e.lineNumber = e; \n" +
           "                e.columnNumber = e; \n" +
           "                e.stack = e; \n" +
           "                throw e; \n" +
           "              } \n" +
           "            }));");
