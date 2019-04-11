// Don't crash on {return:} from onStep in a generator, before the initial suspend.

// This test tries to force-return from each bytecode instruction in a
// generator, up to the initial suspend.

load(libdir + "asserts.js");

let g = newGlobal({newCompartment: true});
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
    dbg.uncaughtExceptionHook = exc => {
      return {throw: "debugger error: " + exc};
    };

    let val = undefined;
    let caught = undefined;
    try {
        val = g.f();
    } catch (exc) {
        caught = exc;
    }

    if (val === undefined) {
        // Tried to force-return before the initial suspend.
        assertEq(caught, "debugger error: TypeError: can't force return from a generator before the initial yield");
        assertEq(ttl, 0);
        return "pass";
    } else {
        // Reached the initial suspend without forcing a return.
        assertEq(typeof val, "object");
        assertEq(val instanceof g.f, true);
        assertEq(ttl, 1);
        return "done";
    }
}

for (let i = 1; test(i) === "pass"; i++) {}
