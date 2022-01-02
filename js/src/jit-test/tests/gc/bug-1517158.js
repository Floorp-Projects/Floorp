gczeal(0);

(function() {
    var g = newGlobal({newCompartment: true});
    g.debuggeeGlobal = this;
    g.eval("(" + function() {
        dbg = new Debugger(debuggeeGlobal);
        dbg.onExceptionUnwind = function(frame, exc) {
            var s = '!';
            for (var f = frame; f; f = f.older)
                if (f.type === "call")
                    s += f.callee.name;
        };
    } + ")();");
    try {
        h();
    } catch (e) {}
    g.dbg.enabled = false;
})();
// jsfunfuzz-generated
startgc(114496726);
