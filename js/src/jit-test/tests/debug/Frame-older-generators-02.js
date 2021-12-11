// Like Frame-older-generators-01.js, but attach the debugger on the fly.
//
// (That is, check that it works even if the debugger never received
// onNewGenerator for the generator, because it wasn't attached at the time.)

let g = newGlobal({newCompartment: true});
g.eval(`
    function f() {
        attach();
        debugger;
    }
    function* gen() {
        f();
        yield 1;
        f();
    }
    async function af() {
        f();
        await 1;
        f();
    }
`);

function test(expected, code) {
    let dbg;
    let hits = 0;
    let genFrame = null;

    g.attach = () => {
        if (dbg === undefined) {
            dbg = Debugger(g);
            dbg.onDebuggerStatement = frame => {
                hits++;
                assertEq(frame.callee.name, "f");
                if (genFrame === null) {
                    genFrame = frame.older;
                } else {
                    assertEq(frame.older, genFrame);
                }
                assertEq(genFrame.callee.name, expected);
            };
        }
    };
    g.eval(code);
    assertEq(hits, 2);
    dbg.removeDebuggee(g);
}

test("gen", "for (var x of gen()) {}");
test("af", "af(); drainJobQueue();");
