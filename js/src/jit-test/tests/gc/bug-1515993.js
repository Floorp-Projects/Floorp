ignoreUnhandledRejections();

function checkGetOffsetsCoverage() {
    var g = newGlobal({newCompartment: true});
    var dbg = Debugger(g);
    var topLevel;
    dbg.onNewScript = function(s) {
        topLevel = s;
    };
    g.eval(`import(() => 1)`);
    topLevel.getChildScripts();
}
checkGetOffsetsCoverage();
gczeal(14, 10);
Object.defineProperty(this, "fuzzutils", {});
