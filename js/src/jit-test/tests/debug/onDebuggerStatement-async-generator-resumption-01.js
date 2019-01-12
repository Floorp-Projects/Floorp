// A Debugger can {return:} from onDebuggerStatement in an async generator.
// A resolved promise for a {value: _, done: true} object is returned.

load(libdir + "asserts.js");

let g = newGlobal({newCompartment: true});
g.eval(`
    async function* f(x) {
        debugger;  // when==0 to force return here
        await x;
        yield 1;
        debugger;  // when==1 to force return here
    }
`);

let exc = null;
let dbg = new Debugger;
let gw = dbg.addDebuggee(g);
function test(when) {
    let hits = 0;
    let outcome = "FAIL";
    dbg.onDebuggerStatement = frame => {
        if (hits++ == when)
            return {return: "ponies"};
    };

    let iter = g.f(0);

    // At the initial suspend.
    assertEq(hits, 0);
    iter.next().then(result => {
        // At the yield point, unless we already force-returned from the first
        // debugger statement.
        assertEq(hits, 1);
        if (when == 0)
            return result;
        assertEq(result.value, 1);
        assertEq(result.done, false);
        return iter.next();
    }).then(result => {
        // After forced return.
        assertEq(hits, when + 1);
        assertEq(result.value, "ponies");
        assertEq(result.done, true);
        outcome = "pass";
    }).catch(e => {
        // An assertion failed.
        exc = e;
    });

    assertEq(hits, 1);
    drainJobQueue();
    if (exc !== null)
        throw exc;
    assertEq(outcome, "pass");
}

for (let i = 0; i < 2; i++) {
    test(i);
}
