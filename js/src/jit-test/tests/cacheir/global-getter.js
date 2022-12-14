// Tests for |this| value passed to getters defined on the global.

function test(useWindowProxy) {
    var g = newGlobal({useWindowProxy});
    g.useWindowProxy = useWindowProxy;
    g.evaluate(`
        var x = 123;
        Object.defineProperty(this, "getX", {get: function() { return this.x; }});
        Object.defineProperty(Object.prototype, "protoGetX", {get: function() { return this.x * 2; }});
        Object.defineProperty(this, "thisIsProxy", {get: function() { return isProxy(this); }});

        function f() {
            for (var i = 0; i < 100; i++) {
                // GetGName
                assertEq(getX, 123);
                assertEq(protoGetX, 246);
                assertEq(thisIsProxy, useWindowProxy);
                // GetProp
                assertEq(globalThis.getX, 123);
                assertEq(globalThis.protoGetX, 246);
                assertEq(globalThis.thisIsProxy, useWindowProxy);
            }
        }
        f();
    `);
}

for (let useWindowProxy of [true, false]) {
    test(useWindowProxy);
}

setJitCompilerOption("ic.force-megamorphic", 1);

for (let useWindowProxy of [true, false]) {
    test(useWindowProxy);
}
