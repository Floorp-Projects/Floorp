var g = newGlobal({newCompartment: true});

g.evaluate(`
    function testInnerFun(defaultArg = 1) {
        function innerFun(expectedThis) { return this; }
        h();
        return innerFun; // To prevent the JIT from optimizing out innerFun.
    }
`);

g.h = function () {
    var res = (new Debugger(g)).getNewestFrame().eval('assertEq(innerFun(), this)');
    assertEq("return" in res, true);
}

g.testInnerFun();
