function f() {
    (function () {
        const otherDebugger = newGlobal({ sameZoneAs: this }).Debugger;
        const dbg = otherDebugger(this);

        const env = dbg.getNewestFrame().callee.environment;
        var ran = false;
        try {
            // this should throw, as tdz_variable is still in the tdz at
            // this point.
            env.setVariable("tdz_variable", 10);
            ran = true;
        } catch (e) { }
        assertEq(ran, false);
    })();

    function bar() {
        return tdz_variable;
    }


    let tdz_variable = 10;
}
f();
