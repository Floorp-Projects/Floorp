var g = newGlobal();
var dbg = Debugger(g);
dbg.onDebuggerStatement = function (frame) {
    try {
        frame.arguments[0].deleteProperty("x");
    } catch (exc) {
        assertEq(exc instanceof ReferenceError, true);
        assertEq(exc.message, "diaf");
        assertEq(exc.fileName, "fail");
        assertEq(exc.lineNumber, 4);

        // Arrant nonsense?  Sure -- but different from lineNumber is all this
        // test exists to verify.  If you're the person to make column numbers
        // actually work, change this accordingly.
        assertEq(exc.columnNumber, 0);
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
