g = newGlobal({newCompartment: true});
g.parent = this;
g.eval("(" + function() {
    Debugger(parent)
        .onExceptionUnwind = function(frame) {
        return frame.eval("");
    };
} + ")()");
m = parseModule(` s1 `);
moduleLink(m);
moduleEvaluate(m);
