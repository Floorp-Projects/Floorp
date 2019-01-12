var g = newGlobal({newCompartment: true});

g.evaluate(`
    function testInnerFun(defaultArg = 1) {
        function innerFun(expectedThis) { return this; }
        h();
    }
`);

g.h = function () {
    var res = (new Debugger(g)).getNewestFrame().eval('assertEq(innerFun(), this)');
    assertEq("return" in res, true);
}

g.testInnerFun();
