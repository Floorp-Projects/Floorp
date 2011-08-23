// Don't crash when a scripted proxy handler throws Error.prototype.

var g = newGlobal('new-compartment');
var dbg = Debugger(g);
dbg.onDebuggerStatement = function (frame) {
    try {
        frame.arguments[0].deleteProperty("x");
    } catch (exc) {
        return;
    }
    throw new Error("deleteProperty should throw");
};

g.eval("function h(x) { debugger; }");
g.eval("h(Proxy.create({delete: function () { throw Error.prototype; }}));");

