gczeal(2);
g = newGlobal({newCompartment: true});
dbg = Debugger(g);
dbg.onNewScript = function() { return function() { return this; } };
schedulegc(10);
g.evaluate("function one() {}", { forceFullParse: true });
g.evaluate(`
           function target () {}
           function two2() {}
           `, { forceFullParse: true });
g.evaluate(`
           function three1() {}
           function three2() {}
           function three3() {}
           `, { forceFullParse: true });
dbg.memory.takeCensus({
  breakdown: {
    by: "coarseType",
    scripts: {
      by: "filename"
    }
  }
});
