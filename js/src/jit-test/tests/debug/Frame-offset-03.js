// frame.offset works in generators.

load(libdir + "asserts.js");

var g = newGlobal({ newCompartment: true });
var dbg = new Debugger(g);
g.eval(`
function* f() {
  yield 11;
  yield 21;
  yield 31;
}
`);

const ranges = [];
let script;
let frame;
dbg.onEnterFrame = function(f) {
  if (frame) {
    assertEq(f, frame);
    assertEq(f.script, script);
  } else {
    frame = f;
    script = f.script;
    assertEq(frame instanceof Debugger.Frame, true);
    assertEq(script instanceof Debugger.Script, true);
  }

  const range = [script.getOffsetMetadata(frame.offset).lineNumber, null];
  ranges.push(range);
  f.onPop = () => {
    range[1] = script.getOffsetMetadata(frame.offset).lineNumber;
  };
};

const it = g.f();

assertEq(ranges.length, 1);
assertEq(ranges[0][0], 2);
assertEq(ranges[0][1], 2);
assertEq(script.getOffsetMetadata(frame.offset).lineNumber, 2);

it.next();

assertEq(ranges.length, 2);
assertEq(ranges[1][0], 2);
assertEq(ranges[1][1], 3);
assertEq(script.getOffsetMetadata(frame.offset).lineNumber, 3);

it.next();

assertEq(ranges.length, 3);
assertEq(ranges[2][0], 3);
assertEq(ranges[2][1], 4);
assertEq(script.getOffsetMetadata(frame.offset).lineNumber, 4);

it.next();

assertEq(ranges.length, 4);
assertEq(ranges[3][0], 4);
assertEq(ranges[3][1], 5);
assertEq(script.getOffsetMetadata(frame.offset).lineNumber, 5);

it.next();

assertEq(ranges.length, 5);
assertEq(ranges[4][0], 5);
assertEq(ranges[4][1], 6);

// The frame has finished evaluating, so the callee is no longer accessible.
assertThrowsInstanceOf(() => frame.offset, Error);
