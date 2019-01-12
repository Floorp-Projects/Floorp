// A debugger can {throw:} from onEnterFrame at any resume point in a generator.
// It closes the generator.

load(libdir + "asserts.js");

let g = newGlobal({newCompartment: true});
g.eval(`
    function* f() { yield 1; }
    var exn = new TypeError("object too hairy");
`);

let dbg = new Debugger;
let gw = dbg.addDebuggee(g);

// Repeat the test for each onEnterFrame event.
// It fires up to three times:
// - when the generator g.f is called;
// - when we enter it to run to `yield 1`;
// - when we resume after the yield to run to the end.
for (let i = 0; i < 3; i++) {
    let hits = 0;
    dbg.onEnterFrame = frame => {
        return hits++ < i ? undefined : {throw: gw.makeDebuggeeValue(g.exn)};
    };
    let genObj;
    assertThrowsValue(
        () => {
            genObj = g.f();
            for (let x of genObj) {}
        },
        g.exn
    );
    assertEq(hits, i + 1);
    if (hits > 1)
        assertEq(genObj.next().done, true);
}
