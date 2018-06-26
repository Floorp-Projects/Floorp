function testBasic() {
    var g = newGlobal({sameCompartmentAs: this});
    g.evaluate("function foo() { return scriptedCallerGlobal(); }");
    for (var i = 0; i < 20; i++)
        assertEq(g.foo(), g);

    g.evaluate("function Obj() {}");
    for (var i = 0; i < 30; i++)
        assertEq(objectGlobal(new g.Obj()), g);
}
testBasic();

function testFunCall() {
    var g = newGlobal({sameCompartmentAs: this});
    g.evaluate("function foo() { return scriptedCallerGlobal(); }");
    for (var i = 0; i < 20; i++)
        assertEq(g.foo.call(1, 2), g);
}
testFunCall();

function testFunApplyArgs() {
    var g = newGlobal({sameCompartmentAs: this});
    g.evaluate("function foo() { return scriptedCallerGlobal(); }");
    for (var i = 0; i < 2000; i++)
        assertEq(g.foo.apply(null, arguments), g);
}
testFunApplyArgs(1, 2);

function testFunApplyArray() {
    var g = newGlobal({sameCompartmentAs: this});
    g.evaluate("function foo() { return scriptedCallerGlobal(); }");
    var arr = [1, 2];
    for (var i = 0; i < 2000; i++)
        assertEq(g.foo.apply(null, arr), g);
}
testFunApplyArray();

function testAccessor() {
    var g = newGlobal({sameCompartmentAs: this});
    g.evaluate("function foo() { return scriptedCallerGlobal(); }");
    var o = {};
    Object.defineProperty(o, "foo", {get: g.foo, set: g.foo});
    for (var i = 0; i < 20; i++) {
        assertEq(o.foo, g);
        o.foo = 1;
    }
}
testAccessor();

function testGenerator() {
    var thisGlobalGen = function*() {};
    var thisGlobalGenObj = thisGlobalGen();

    var g = newGlobal({sameCompartmentAs: this});
    g.evaluate("function* gen() { for (var i = 0; i < 2; i++) yield new Array(1, 2); }");
    for (var i = 0; i < 20; i++) {
        var o = g.gen();
        for (var j = 0; j < 2; j++) {
            var res = thisGlobalGenObj.next.call(o);
            assertEq(objectGlobal(res), g);
            assertEq(Array.isArray(res.value), true);
            assertEq(objectGlobal(res.value), g);
        }
    }
}
testGenerator();
