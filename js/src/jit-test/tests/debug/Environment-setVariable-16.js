(function () {
    const otherDebugger = newGlobal({ sameZoneAs: this }).Debugger;
    const dbg = otherDebugger(this);

    const env = dbg.getNewestFrame().callee.environment;
    var ran = false;
    try {
        env.setVariable("tdz_variable", 10);
        ran = true;
    } catch (e) { }
    assertEq(ran, false);
})();

// We shouldn't crash when initializing this.
let tdz_variable = 10;

