// Test shouldAvoidSideEffects flag.

const g = newGlobal({newCompartment: true});
const dbg = Debugger(g);
const gdbg = dbg.addDebuggee(g);

gdbg.executeInGlobal(`
var obj, result, reachedNextLine;
`);

dbg.shouldAvoidSideEffects = false;
assertEq(dbg.shouldAvoidSideEffects, false);

let result = gdbg.executeInGlobal(`
result = undefined;
reachedNextLine = false;

obj = createSideEffectfulResolveObject();
result = obj.test;
reachedNextLine = true;
"finished";
`);
assertEq(g.result, 42);
assertEq(g.reachedNextLine, true);
assertEq(result.return, "finished");

dbg.shouldAvoidSideEffects = true;
assertEq(dbg.shouldAvoidSideEffects, true);

result = gdbg.executeInGlobal(`
result = undefined;
reachedNextLine = false;

obj = createSideEffectfulResolveObject();
result = obj.test;
reachedNextLine = true;
"finished";
`);
assertEq(g.result, undefined);
assertEq(g.reachedNextLine, false);
assertEq(result, null);

// Resolve after abort.
dbg.shouldAvoidSideEffects = false;
assertEq(dbg.shouldAvoidSideEffects, false);

result = gdbg.executeInGlobal(`
result = undefined;
reachedNextLine = false;

result = obj.test;
reachedNextLine = true;
"finished";
`);
assertEq(g.result, 42);
assertEq(g.reachedNextLine, true);
assertEq(result.return, "finished");
