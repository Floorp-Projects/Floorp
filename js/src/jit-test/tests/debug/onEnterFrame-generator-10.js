// Bug 1556033. Test behavior of onEnterFrame "return" completion
// values during explicit .next() calls.

let g = newGlobal({newCompartment: true});
g.eval(`function* f(x) { }`);
let dbg = new Debugger(g);

let it = g.f();

dbg.onEnterFrame = () => ({ return: "exit" });
const result = it.next();
assertEq(result.value, "exit");
assertEq(result.done, true);

const result2 = it.next();
assertEq(result2.value, undefined);
assertEq(result2.done, true);
