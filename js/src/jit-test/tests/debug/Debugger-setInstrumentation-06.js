// Test instrumentation on property and element accesses.

load(libdir + 'eqArrayHelper.js');

var g = newGlobal({ newCompartment: true });
var dbg = Debugger(g);
var gdbg = dbg.addDebuggee(g);

function setScriptId(script) {
  script.setInstrumentationId(0);
  script.getChildScripts().forEach(setScriptId);
}

dbg.onNewScript = setScriptId;

function getOffsetLine(scriptId, offset) {
  const script = allScripts[scriptId];
  return script.getOffsetMetadata(offset).lineNumber;
}

const executedOperations = [];

function propertyRead(script, offset, obj, prop) {
  executedOperations.push(`read:${obj.label}:${prop}`);
}

function propertyWrite(script, offset, obj, prop, rhs) {
  executedOperations.push(`write:${obj.label}:${prop}:${typeof rhs}`);
}

function callback(kind, ...args) {
  switch (kind) {
  case "getProperty":
  case "getElement":
    propertyRead(...args);
    break;
  case "setProperty":
  case "setElement":
    propertyWrite(...args);
  }
}

gdbg.setInstrumentation(
  gdbg.makeDebuggeeValue(callback),
  ["getProperty", "getElement", "setProperty", "setElement"]
);

function testFunction(fn, expected) {
  gdbg.setInstrumentationActive(true);
  for (var i = 0; i < 5; i++) {
    executedOperations.length = 0;
    fn();
    assertEqArray(executedOperations, expected);
  }
  gdbg.setInstrumentationActive(false);
}

g.eval(`
function basic(o, i) {
  o.x = o.x + 1;
  o[i] = o[i] + 1;
}
`);

testFunction(() => g.basic({ label: 0}, 3), [
  "read:0:x",
  "write:0:x:number",
  "read:0:3",
  "write:0:3:number",
]);

g.eval(`
function incdec(o, i) {
  o.x++;
  o[i]++;
}
`);

testFunction(() => g.incdec({ label: 0}, 3), [
  "read:0:x",
  "write:0:x:number",
  "read:0:3",
  "write:0:3:number",
]);

g.eval(`
function destructure({ f, g }) {
  const { h, i } = f;
}
`);

testFunction(() => g.destructure({ label: 0, f: { label: 1 }}, 3), [
  "read:0:f",
  "read:0:g",
  "read:1:h",
  "read:1:i",
]);

g.eval(`
function destructureArray([a, b]) {}
`);

// Array destructuring uses iterators instead of getElement accesses,
// and no property accesses will be captured by instrumentation.
testFunction(() => g.destructureArray([1, 2]), []);
