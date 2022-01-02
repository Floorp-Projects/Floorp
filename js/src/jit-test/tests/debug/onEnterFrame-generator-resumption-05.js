// {return:} from the initial onEnterFrame for a generator is an error.

load(libdir + "asserts.js");

let g = newGlobal({newCompartment: true});
g.values = [1, 2, 3];
g.eval(`function* f(arr=values) { yield* arr; }`);

let dbg = Debugger(g);

let hits = 0;
dbg.onEnterFrame = frame => {
    assertEq(frame.callee.name, "f");
    hits++;
    return {return: 123};
};
dbg.uncaughtExceptionHook = exc => {
  assertEq(exc instanceof TypeError, true);
  return {throw: "REJECTED"};
}
assertThrowsValue(g.f, "REJECTED");
assertEq(hits, 1);
