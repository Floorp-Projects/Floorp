g = newGlobal();
g.parent = this;
g.eval("(" + function() {
    Debugger(parent)
        .onExceptionUnwind = function(frame) {
        return frame.eval("");
    };
} + ")()");
m = parseModule(` s1 `);
instantiateModule(m);
evaluateModule(m);
