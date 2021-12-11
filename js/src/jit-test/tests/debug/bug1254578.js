// |jit-test| slow; skip-if: !('oomTest' in this)

var g = newGlobal({newCompartment: true});
g.debuggeeGlobal = this;
g.eval("(" + function() {
    dbg = new Debugger(debuggeeGlobal);
    dbg.onExceptionUnwind = function(frame, exc) {
        var s = '!';
        for (var f = frame; f; f = f.older)
            debuggeeGlobal.log += s;
    };
} + ")();");
var dbg = new Debugger;
dbg.onNewGlobalObject = function(global) {
    get.seen = true;
};
oomTest(function() {
    newGlobal({sameZoneAs: this})
});
