// Testing the promise property on async function frames.

load(libdir + 'asserts.js');

var g = newGlobal({ newCompartment: true });
var dbg = Debugger(g);
g.eval(`
async function f() {
  debugger;
  await Promise.resolve();
  debugger;
}
`);

let frame;
const promises = [];
dbg.onEnterFrame = function(f) {
  frame = f;
  promises.push(["start", frame.asyncPromise]);

  dbg.onEnterFrame = function(f) {
    assertEq(f, frame);
    promises.push(["resume", frame.asyncPromise]);
  };

  frame.onPop = function() {
    promises.push(["suspend", frame.asyncPromise]);

    frame.onPop = function() {
      promises.push(["finish", frame.asyncPromise]);
    };
  };

  dbg.onDebuggerStatement = function(f) {
    assertEq(f, frame);
    promises.push(["dbg", frame.asyncPromise]);
  };
};

(async () => {
  const resultPromise = g.f();

  const framePromise = frame.asyncPromise;
  assertEq(framePromise instanceof Debugger.Object, true);
  assertEq(framePromise.unsafeDereference(), resultPromise);

  assertEq(promises.length, 3);
  assertEq(promises[0][0], "start");
  assertEq(promises[0][1], null);

  assertEq(promises[1][0], "dbg");
  assertEq(promises[1][1], framePromise);

  assertEq(promises[2][0], "suspend");
  assertEq(promises[2][1], framePromise);

  await resultPromise;

  // Frame has terminated, so accessing the property throws.
  assertThrowsInstanceOf(() => frame.asyncPromise, Error);

  assertEq(promises.length, 6);

  assertEq(promises[3][0], "resume");
  assertEq(promises[3][1], framePromise);

  assertEq(promises[4][0], "dbg");
  assertEq(promises[4][1], framePromise);

  assertEq(promises[5][0], "finish");
  assertEq(promises[5][1], framePromise);
})();
