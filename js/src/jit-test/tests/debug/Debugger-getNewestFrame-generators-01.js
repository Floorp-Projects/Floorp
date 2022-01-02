// Generator/async frames can be created and revived by calling Debugger.getNewestFrame().
//
// Modified copy of Frame-older-generators-01.js.

let g = newGlobal({newCompartment: true});
g.eval(`
    function* gen() {
        f();
        yield 1;
        f();
    }
    function* genDefaults(x=f()) {
        f();
    }
    async function af() {
        f();
        await 1;
        f();
    }
    async function afDefaults(x=f()) {
        await 1;
        f();
    }
`);

function test(expected, code) {
    let dbg = Debugger(g);
    let hits = 0;
    let genFrame = null;
    g.f = () => {
        hits++;
        let frame = dbg.getNewestFrame();
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
test("genDefaults", "for (var x of genDefaults()) {}");
test("af", "af(); drainJobQueue();");
test("afDefaults", "afDefaults(); drainJobQueue();")
