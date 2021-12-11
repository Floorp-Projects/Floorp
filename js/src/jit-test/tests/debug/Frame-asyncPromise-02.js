// Test the promise property on normal function frames.

load(libdir + 'asserts.js');

var g = newGlobal({ newCompartment: true });
var dbg = Debugger(g);
g.eval(`
function f() {
  debugger;
}
`);

let frame;
const promises = [];
dbg.onDebuggerStatement = function(f) {
  frame = f;
  promises.push(frame.asyncPromise);
};

const resultPromise = g.f();

// Frame has terminated, so accessing the property throws.
assertThrowsInstanceOf(() => frame.asyncPromise, Error);

assertEq(promises.length, 1);
assertEq(promises[0], undefined);
