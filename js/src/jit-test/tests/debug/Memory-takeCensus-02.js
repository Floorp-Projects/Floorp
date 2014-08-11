// Debugger.Memory.prototype.takeCensus behaves plausibly as we allocate and drop objects.

load(libdir + 'census.js');

var dbg = new Debugger;
var census0 = dbg.memory.takeCensus();
Census.walkCensus(census0, "census0", Census.assertAllZeros);

var g1 = newGlobal();
g1.eval('var a = [];');
g1.eval('function add(f) { a.push({}); a.push(f ? (() => undefined) : null); }');
g1.eval('function remove() { a.pop(); a.pop(); }');
g1.add();
g1.remove();

// Adding a global shouldn't cause any counts to *decrease*.
dbg.addDebuggee(g1);
var census1 = dbg.memory.takeCensus();
Census.walkCensus(census1, "census1", Census.assertAllNotLessThan(census0));

function pointCheck(label, lhs, rhs, objComp, funComp) {
  print(label);
  assertEq(objComp(lhs.objects.Object.count, rhs.objects.Object.count), true);
  assertEq(funComp(lhs.objects.Function.count, rhs.objects.Function.count), true);
}

function eq(lhs, rhs) { return lhs === rhs; }
function lt(lhs, rhs) { return lhs < rhs; }
function gt(lhs, rhs) { return lhs > rhs; }

// As we increase the number of reachable objects, the census should
// reflect that.
g1.add(false);
var census2 = dbg.memory.takeCensus();
pointCheck("census2", census2, census1, gt, eq);

g1.add(true);
var census3 = dbg.memory.takeCensus();
pointCheck("census3", census3, census2, gt, gt);

g1.add(false);
var census4 = dbg.memory.takeCensus();
pointCheck("census4", census4, census3, gt, eq);

// As we decrease the number of reachable objects, the census counts should go
// down. Note that since the census does its own reachability analysis, we don't
// need to GC here to see the counts drop.
g1.remove();
var census5 = dbg.memory.takeCensus();
pointCheck("census5", census5, census4, lt, eq);

g1.remove();
var census6 = dbg.memory.takeCensus();
pointCheck("census6", census6, census5, lt, lt);
