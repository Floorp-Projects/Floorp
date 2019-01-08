// |jit-test| skip-if: !('oomTest' in this)

ignoreUnhandledRejections();

(function () {
    g = newGlobal();
    g.parent = this;
    g.eval("(function() { var dbg = Debugger(parent); dbg.onEnterFrame = function() {} } )")
    ``;
    oomTest(async function() {}, {expectExceptionOnFailure: false});
})()
