gczeal(2);
g = newGlobal();
dbg = Debugger(g);
dbg.onNewScript = function() function() this;
schedulegc(10);
g.eval("setLazyParsingDisabled(true)");
g.evaluate("function one() {}");
g.evaluate(`
           function target () {}
           function two2() {}
           `, {});
g.evaluate(`
           function three1() {}
           function three2() {}
           function three3() {}
           `, {});
dbg.memory.takeCensus({
  breakdown: {
    by: "coarseType",
    scripts: {
      by: "filename"
    }
  }
});
