// frame.offset works in async fns.

load(libdir + "asserts.js");

var g = newGlobal({ newCompartment: true });
var dbg = new Debugger(g);
g.eval(`
async function f() {
  await Promise.resolve();
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

(async () => {
  const promise = g.f();

  assertEq(ranges.length, 1);
  assertEq(ranges[0][0], 2);
  assertEq(ranges[0][1], 3);
  assertEq(script.getOffsetMetadata(frame.offset).lineNumber, 3);

  await promise;

  assertEq(ranges.length, 2);
  assertEq(ranges[1][0], 3);
  assertEq(ranges[1][1], 4);

  // The frame has finished evaluating, so the callee is no longer accessible.
  assertThrowsInstanceOf(() => frame.offset, Error);
})();
