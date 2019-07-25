// Test main/entry/exit point instrumentation.

load(libdir + 'eqArrayHelper.js');

ignoreUnhandledRejections();

var g = newGlobal({ newCompartment: true });
var dbg = Debugger(g);
var gdbg = dbg.addDebuggee(g);

var allScripts = [];

function setScriptId(script) {
  script.setInstrumentationId(allScripts.length);
  allScripts.push(script);

  script.getChildScripts().forEach(setScriptId);
}

dbg.onNewScript = setScriptId;

const executedPoints = [];
function hitPoint(kind, scriptId) {
  const name = allScripts[scriptId].displayName;
  executedPoints.push(`${name}:${kind}`);
}

gdbg.setInstrumentation(
  gdbg.makeDebuggeeValue(hitPoint),
  ["main", "entry", "exit"]
);

function testFunction(fn, expected) {
  for (var i = 0; i < 3; i++) {
    try { fn(); } catch (e) {}
    drainJobQueue();
  }

  assertEq(executedPoints.length, 0);
  gdbg.setInstrumentationActive(true);

  for (var i = 0; i < 5; i++) {
    executedPoints.length = 0;
    try { fn(); } catch (e) {}
    drainJobQueue();
    assertEqArray(executedPoints, expected);
  }

  executedPoints.length = 0;
  gdbg.setInstrumentationActive(false);
}

g.eval(`
function basic() {
  var a = 0;
  a++;
}
`);

testFunction(() => g.basic(), ["basic:main", "basic:exit"]);

g.eval(`
function thrower(v) {
  if (v) {
    throw new Error();
  }
}
`);

testFunction(() => g.thrower(0), ["thrower:main", "thrower:exit"]);
testFunction(() => g.thrower(1), ["thrower:main"]);

g.eval(`
function* yielder() { yield 1; }
function iterator() {
  for (var n of yielder()) {}
}
`);

testFunction(() => g.iterator(), [
  "iterator:main",
  "yielder:main", "yielder:exit",
  "yielder:entry", "yielder:exit",
  "yielder:entry", "yielder:exit",
  "iterator:exit"
]);

g.eval(`
function promiser(n) {
  return new Promise(function promiseInternal(resolve, reject) {
    if (n) {
      reject(new Error());
    } else {
      resolve();
    }
  });
}
function f1() {}
async function asyncer(n) { await promiser(n) }
async function asyncerCatch(n) { try { await promiser(n) } catch (e) { f1() } }
async function asyncerFinally(n) { try { await promiser(n) } finally { f1() } }
`);

testFunction(() => g.asyncer(0), [
  "asyncer:main",
  "promiser:main", "promiseInternal:main", "promiseInternal:exit", "promiser:exit",
  "asyncer:exit",
  "asyncer:entry", "asyncer:exit"
]);
testFunction(() => g.asyncer(1), [
  "asyncer:main",
  "promiser:main", "promiseInternal:main", "promiseInternal:exit", "promiser:exit",
  "asyncer:exit",
  "asyncer:entry", "asyncer:exit"
]);
testFunction(() => g.asyncerCatch(1), [
  "asyncerCatch:main",
  "promiser:main", "promiseInternal:main", "promiseInternal:exit", "promiser:exit",
  "asyncerCatch:exit",
  "asyncerCatch:entry", "f1:main", "f1:exit", "asyncerCatch:exit"
]);
testFunction(() => g.asyncerFinally(1), [
  "asyncerFinally:main",
  "promiser:main", "promiseInternal:main", "promiseInternal:exit", "promiser:exit",
  "asyncerFinally:exit",
  "asyncerFinally:entry", "f1:main", "f1:exit", "asyncerFinally:entry", "asyncerFinally:exit"
]);
