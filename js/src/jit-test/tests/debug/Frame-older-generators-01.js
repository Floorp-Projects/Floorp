// Generator/async frames can be created by following .older.
//
// The goal here is to get some test coverage creating generator Frame objects
// at some time other than when firing onEnterFrame. Here they're created after
// the initial yield.

let g = newGlobal({newCompartment: true});
g.eval(`
    function f() {
        debugger;
    }
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
    g.eval(code);
    assertEq(hits, 2);
    dbg.removeDebuggee(g);
}

test("gen", "for (var x of gen()) {}");
test("genDefaults", "for (var x of genDefaults()) {}");
test("af", "af(); drainJobQueue();");
test("afDefaults", "afDefaults(); drainJobQueue();")
