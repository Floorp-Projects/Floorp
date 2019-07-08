// Test breakpoint site instrumentation functionality with a variety of control
// flow constructs.

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
  for (var i = 0; i < 3; i++) {
    fn();
  }

  assertEq(executedLines.length, 0);
  gdbg.setInstrumentationActive(true);

  for (var i = 0; i < 5; i++) {
    executedLines.length = 0;
    fn();
    assertEqArray(executedLines, expected);
  }

  executedLines.length = 0;
  gdbg.setInstrumentationActive(false);
}

g.eval(`
function basic() {
  var a = 0;
  a++;
}
`);

testFunction(() => g.basic(),
             [3, 4, 5]);

g.eval(`
function cforloop() {
  for (var i = 0;
       i < 5;
       i++) {
  }
}
`);

testFunction(() => g.cforloop(),
             [3, 4, 5, 4, 5, 4, 5, 4, 5, 4, 5, 4, 7]);

g.eval(`
function ifstatement(n) {
  if (n) {
    return 1;
  }
  return 0;
}
`);

testFunction(() => g.ifstatement(true), [3, 4]);
testFunction(() => g.ifstatement(false), [3, 6]);

g.eval(`
function whileloop(n) {
  while (n < 3) {
    n++;
  }
}
`);

testFunction(() => g.whileloop(1),
             [3, 4, 3, 4, 3, 6]);

g.eval(`
function dowhileloop(n) {
  do {
    if (++n == 3) {
      break;
    }
  } while (true);
}
`);

testFunction(() => g.dowhileloop(1),
             [4, 7, 4, 5, 8]);

g.eval(`
function forinloop(v) {
  for (let i in v) {
    var a = i;
  }
}
`);

testFunction(() => g.forinloop([1,2,3]),
             [3, 4, 4, 4, 6]);

g.eval(`
function forofloop(v) {
  for (const i of v) {
    var a = i;
  }
}
`);

testFunction(() => g.forofloop([1,2,3]),
             [3, 4, 4, 4, 6]);

g.eval(`
function normalswitch(v) {
  switch (v) {
    case 3:
      return 0;
    case "three":
      return 1;
    default:
      return 2;
  }
}
`);

testFunction(() => g.normalswitch(3), [3, 5]);
testFunction(() => g.normalswitch("three"), [3, 7]);
testFunction(() => g.normalswitch(2), [3, 9]);

g.eval(`
function tableswitch(v) {
  switch (v) {
    case 0:
    case 1:
    case 2:
      break;
    case 3:
      return 1;
    default:
      return 2;
  }
}
`);

testFunction(() => g.tableswitch(0), [3, 7, 13]);
testFunction(() => g.tableswitch(3), [3, 9]);
testFunction(() => g.tableswitch(5), [3, 11]);

g.eval(`
function trycatch(f) {
  try {
    f();
  } catch (e) {
    return e;
  }
}
`);

testFunction(() => g.trycatch(() => { throw 3 }), [4, 6]);
testFunction(() => g.trycatch(() => {}), [4, 8]);

g.eval(`
function tryfinally(f) {
  var a;
  try {
    f();
  } finally {
    a = 0;
  }
  a = 1;
}
`);

testFunction(() => {
  try { g.tryfinally(() => { throw 3 }); } catch (e) {}
}, [5, 7]);
testFunction(() => g.tryfinally(() => {}), [5, 7, 9, 10]);
