// Don't crash when a scripted proxy handler throws Error.prototype.

var g = newGlobal();
var dbg = Debugger(g);
dbg.onDebuggerStatement = function (frame) {
    try {
        frame.arguments[0].deleteProperty("x");
    } catch (exc) {
        return;
    }
    throw new Error("deleteProperty should throw");
};

g.eval("function h(obj) { debugger; }");
g.eval("h(new Proxy({}, { deleteProperty() { throw Error.prototype; }}));");

