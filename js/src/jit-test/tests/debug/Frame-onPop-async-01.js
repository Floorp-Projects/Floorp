// When an async function awaits, if Frame.onPop processes microtasks,
// the async function itself will not run. It'll run later.
//
// This is a reentrancy test, like Frame-onPop-generators-03.

let g = newGlobal();
g.log = "";
g.eval(`
    async function f() {
        log += "1";
        debugger;
        log += "2";
        await Promise.resolve(3);
        log += "3";
        return "ok";
    }
`);

let dbg = Debugger(g);
dbg.onDebuggerStatement = frame => {
    frame.onPop = completion => {
        // What we are really testing is that when onPop is called, we have not
        // yet thrown this async function activation back into the hopper.
        g.log += 'A';
        drainJobQueue();
        g.log += 'B';
    };
};

let status = "FAIL - g.f() did not resolve";
g.f().then(value => { status = value; });
assertEq(g.log, "12AB");
drainJobQueue();
assertEq(g.log, "12AB3");
assertEq(status, "ok");
