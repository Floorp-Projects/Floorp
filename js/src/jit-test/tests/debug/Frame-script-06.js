// Frame.prototype.script for async generator frames.

load(libdir + "asserts.js");

var g = newGlobal({ newCompartment: true });
var dbg = new Debugger(g);
g.eval(`
async function* f() {
  await Promise.resolve();
}
`);

let frame;
let script;
dbg.onEnterFrame = function(f) {
  frame = f;
  script = frame.script;
};

const it = g.f();

assertEq(frame instanceof Debugger.Frame, true);
assertEq(script instanceof Debugger.Script, true);
assertEq(frame.script, script);

let lastFrame = frame;
let lastScript = script;
frame = null;
script = null;

let promise = it.next();

assertEq(frame, lastFrame);
assertEq(script, lastScript);
assertEq(frame.script, script);

lastFrame = frame;
lastScript = script;
frame = null;
script = null;

promise.then(() => {
  assertEq(frame, lastFrame);
  assertEq(script, lastScript);

  // The frame has finished evaluating, so the script is no longer accessible.
  assertThrowsInstanceOf(() => frame.script, Error);
});
