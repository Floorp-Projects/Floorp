// Don't crash on {return:} from onStep in a generator, before the initial suspend.

// This test forces a return from each bytecode instruction in a generator, up
// to the initial suspend.

load(libdir + "asserts.js");

let g = newGlobal();
g.values = [1, 2, 3];
g.eval(`function* f(arr=values) { yield* arr; }`);

let dbg = Debugger(g);

function test(ttl) {
    let hits = 0;
    dbg.onEnterFrame = frame => {
        assertEq(frame.callee.name, "f");
        frame.onStep = () => {
            if (--ttl === 0)
                return {return: 123};
        };
    };
    let val = g.f();
    if (typeof val === "object") {
        // Reached the initial suspend without forcing a return.
        assertEq(ttl, 1);
        return "done";
    }

    // Forced a return before the initial suspend.
    assertEq(val, 123);
    assertEq(ttl, 0);
    return "pass";
}

for (let i = 1; test(i) === "pass"; i++) {}
