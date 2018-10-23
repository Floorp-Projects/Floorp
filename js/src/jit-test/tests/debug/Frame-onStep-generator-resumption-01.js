// The debugger can force an early return from any instruction before the initial yield.

let g = newGlobal();
g.eval(`
  function* f() {
    yield 1;
  }
`);

function test(ttl) {
    let dbg = new Debugger(g);
    let exiting = false;  // we ran out of time-to-live and have forced return
    let done = false;  // we reached the initial yield without forced return
    dbg.onEnterFrame = frame => {
        assertEq(frame.callee.name, "f");
        frame.onEnterFrame = undefined;
        frame.onStep = () => {
            if (ttl == 0) {
                exiting = true;
                // Forced return here causes the generator object, if any, not
                // to be exposed.
                return {return: "ponies"};
            }
            ttl--;
        };
        frame.onPop = completion => {
            if (!exiting)
                done = true;
        };
    };

    let result = g.f();
    if (done)
        assertEq(result instanceof g.f, true);
    else
        assertEq(result, "ponies");

    dbg.enabled = false;
    return done;
}

for (let ttl = 0; !test(ttl); ttl++) {}
