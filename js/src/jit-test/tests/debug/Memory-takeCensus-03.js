// Debugger.Memory.prototype.takeCensus behaves plausibly as we add and remove debuggees.

load(libdir + 'census.js');

var dbg = new Debugger;

var census0 = dbg.memory.takeCensus();
Census.walkCensus(census0, "census0", Census.assertAllZeros);

var g1 = newGlobal();
dbg.addDebuggee(g1);
var census1 = dbg.memory.takeCensus();
Census.walkCensus(census1, "census1", Census.assertAllNotLessThan(census0));

var g2 = newGlobal();
dbg.addDebuggee(g2);
var census2 = dbg.memory.takeCensus();
Census.walkCensus(census2, "census2", Census.assertAllNotLessThan(census1), new Set(["bytes"]));

dbg.removeDebuggee(g2);
var census3 = dbg.memory.takeCensus();
Census.walkCensus(census3, "census3", Census.assertAllEqual(census1), new Set(["bytes"]));

dbg.removeDebuggee(g1);
var census4 = dbg.memory.takeCensus();
Census.walkCensus(census4, "census4", Census.assertAllEqual(census0));
