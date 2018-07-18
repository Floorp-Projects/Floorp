// {return:} from the initial onEnterFrame for a generator replaces the
// generator object that's normally returned to the caller.

load(libdir + "asserts.js");

let g = newGlobal();
g.values = [1, 2, 3];
g.eval(`function* f(arr=values) { yield* arr; }`);

let dbg = Debugger(g);

let hits = 0;
dbg.onEnterFrame = frame => {
    assertEq(frame.callee.name, "f");
    hits++;
    return {return: 123};
};
assertEq(g.f(), 123);
assertEq(hits, 1);
