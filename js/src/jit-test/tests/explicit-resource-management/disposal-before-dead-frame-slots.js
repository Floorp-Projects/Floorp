// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management")

load(libdir + "asserts.js");

const g = newGlobal({ newCompartment: true });
const dbg = new Debugger(g);

let called = false;

dbg.onEnterFrame = frame => {
  if (frame.callee?.name === "dispose") {
    const vVal = frame.eval("v");
    assertEq(vVal.return, 10);
    called = true;
  }
};

g.eval(`
function* f() {
  let v = 10;

  using d = {
    [Symbol.dispose]: function dispose() {
    }
  };
  return;
}
f().next();
`);

assertEq(called, true);
