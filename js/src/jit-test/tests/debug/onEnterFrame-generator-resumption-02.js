// A Debugger can {return:} from onEnterFrame at any resume point in a generator.
// Force-returning closes the generator.

load(libdir + "asserts.js");

let g = newGlobal({newCompartment: true});
g.values = [1, 2, 3];
g.eval(`function* f() { yield* values; }`);

let dbg = Debugger(g);

// onEnterFrame will fire up to 5 times.
// - once for the initial call to g.f();
// - four times at resume points:
//   - initial resume at the top of the generator body
//   - resume after yielding 1
//   - resume after yielding 2
//   - resume after yielding 3 (this resumption will run to the end).
// This test ignores the initial call and focuses on resume points.
for (let i = 1; i < 5; i++) {
    let hits = 0;
    dbg.onEnterFrame = frame => {
        return hits++ < i ? undefined : {return: "we're done here"};
    };

    let genObj = g.f();
    let actual = [];
    while (true) {
        let r = genObj.next();
        if (r.done) {
            assertDeepEq(r, {value: "we're done here", done: true});
            break;
        }
        actual.push(r.value);
    }
    assertEq(hits, i + 1);
    assertDeepEq(actual, g.values.slice(0, i - 1));
    assertDeepEq(genObj.next(), {value: undefined, done: true});
}
