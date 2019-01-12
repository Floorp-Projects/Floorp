// Generator/async frames can be created and revived by the onExceptionUnwind hook.
//
// Modified copy of Frame-older-generators-01.js.

let g = newGlobal({newCompartment: true});
g.eval(`
    function* gen() {
        try { throw new Error("bad"); } catch { }
        yield 1;
        try { throw new Error("bad"); } catch { }
    }
    async function af() {
        try { throw new Error("bad"); } catch { }
        await 1;
        try { throw new Error("bad"); } catch { }
    }
`);

function test(expected, code) {
    let dbg = Debugger(g);
    let hits = 0;
    let genFrame = null;
    dbg.onExceptionUnwind = frame => {
        hits++;
        if (genFrame === null) {
            genFrame = frame;
        } else {
            assertEq(frame, genFrame);
        }
        assertEq(genFrame.callee.name, expected);
    }

    g.eval(code);
    assertEq(hits, 2);
    dbg.removeDebuggee(g);
}

test("gen", "for (var x of gen()) {}");
test("af", "af(); drainJobQueue();");
