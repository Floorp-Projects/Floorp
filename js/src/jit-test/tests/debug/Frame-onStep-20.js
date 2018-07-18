// Stepping should always pause in a frame between two function calls.

let g = newGlobal();
g.evaluate(`
    class X {
        constructor() { this._p = 0; }
        m() { return this; }
        get p() { return this._p; }
        set p(value) { this._p = value; }
    }
    let x = new X;

    function f() { return 1; }
    function inc(x) { return x + 1; }
`);

let dbg = Debugger(g);

// `code` is a snippet of JS that performs two JS calls.
function test(code) {
    let hits = 0;
    let log = "";
    dbg.onEnterFrame = frame => {
        if (hits++ === 0)
            frame.onStep = () => { log += "s"; };
        else
            log += "E";
    };

    g.eval(code);
    assertEq(log.includes("EE"), false, "should have received onStep between onEnterFrame events");
    assertEq(log.match(/^s+Es+Es*$/) !== null, true,
             "should get two calls, with steps before, between, and possibly after");
}

test("f(); f();");
test("f() + f()");
test("inc(f())");
test("x.m().m()");
test("new X().m()");
test("x.p = x.p");  // getter, then setter
