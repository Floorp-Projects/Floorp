// Tests for |this| value passed to setters defined on the global.

function test(useWindowProxy) {
    var g = newGlobal({useWindowProxy});
    g.useWindowProxy = useWindowProxy;
    g.evaluate(`
        var x = 123;
        Object.defineProperty(this, "checkX",
                              {set: function(v) { assertEq(this.x, v); }});
        Object.defineProperty(Object.prototype, "protoCheckX",
                              {set: function(v) { assertEq(this.x * 2, v); }});
        Object.defineProperty(this, "checkThisIsProxy",
                              {set: function(v) { assertEq(isProxy(this), v); }});

        function f() {
            for (var i = 0; i < 100; i++) {
                // SetGName
                checkX = 123;
                protoCheckX = 246;
                checkThisIsProxy = useWindowProxy;
                // SetProp
                globalThis.checkX = 123;
                globalThis.protoCheckX = 246;
                globalThis.checkThisIsProxy = useWindowProxy;
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
