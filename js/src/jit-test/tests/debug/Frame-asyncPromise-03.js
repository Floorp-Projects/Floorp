// Test the promise property on generator function frames.

load(libdir + 'asserts.js');

var g = newGlobal({ newCompartment: true });
var dbg = Debugger(g);
g.eval(`
function* f() {
  debugger;
  yield;
  debugger;
}
`);

let frame;
const promises = [];
dbg.onDebuggerStatement = function(f) {
  frame = f;
  promises.push(frame.asyncPromise);
};

const it = g.f();

assertEq(promises.length, 0);

it.next();

assertEq(frame.asyncPromise, undefined);

assertEq(promises.length, 1);
assertEq(promises[0], undefined);

it.next();

// Frame has terminated, so accessing the property throws.
assertThrowsInstanceOf(() => frame.asyncPromise, Error);

assertEq(promises.length, 2);
assertEq(promises[1], undefined);
