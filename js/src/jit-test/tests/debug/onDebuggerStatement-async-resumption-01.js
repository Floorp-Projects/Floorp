// A Debugger can {return:} from onDebuggerStatement in an async function.
// The async function's promise is resolved with the returned value.

load(libdir + "asserts.js");

let g = newGlobal();
g.eval(`
    async function f(x) {
        debugger;  // when==0 to force return here
        await x;
        debugger;  // when==1 to force return here
    }
`);

let dbg = new Debugger;
let gw = dbg.addDebuggee(g);
function test(when, what, expected) {
    let hits = 0;
    let result = "FAIL";
    dbg.onDebuggerStatement = frame => {
        if (hits++ == when)
            return {return: gw.makeDebuggeeValue(what)};
    };
    g.f(0).then(x => { result = x; });
    assertEq(hits, 1);
    drainJobQueue();
    assertEq(hits, when + 1);
    assertEq(result, expected);
}

for (let i = 0; i < 2; i++) {
    test(i, "ok", "ok");
    test(i, g.Promise.resolve(37), 37);
}
