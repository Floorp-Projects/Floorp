var g = newGlobal({ newCompartment: true });
var dbg = Debugger(g);

dbg.collectCoverageInfo = true;

g.eval(`
    function fn() {}
    fn();
`);

dbg = null;
gc();

relazifyFunctions();
