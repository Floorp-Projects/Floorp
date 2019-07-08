// Test instrumentation functionality when using generators and async functions.

load(libdir + 'eqArrayHelper.js');

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

function getOffsetLine(scriptId, offset) {
  const script = allScripts[scriptId];
  return script.getOffsetMetadata(offset).lineNumber;
}

const executedLines = [];
gdbg.setInstrumentation(
  gdbg.makeDebuggeeValue((kind, script, offset) => {
    executedLines.push(getOffsetLine(script, offset));
  }),
  ["breakpoint"]
);

function testFunction(fn, expected) {
  gdbg.setInstrumentationActive(true);

  for (var i = 0; i < 5; i++) {
    executedLines.length = 0;
    fn();
    assertEqArray(executedLines, expected);
  }

  gdbg.setInstrumentationActive(false);
}

g.eval(`
async function asyncfun() {
  await Promise.resolve(0);
  await Promise.resolve(1);
  var a = 0;
  await Promise.resolve(2);
  a++;
}
`);

testFunction(() => {
  async function f() { await g.asyncfun(); }
  f();
  drainJobQueue();
}, [3, 3, 4, 4, 5, 6, 6, 7]);

g.eval(`
function *generator() {
  yield 1;
  var a = 0;
  yield 2;
  a++;
}
`);

testFunction(() => {
  for (const i of g.generator()) {}
}, [3, 4, 5, 6]);
