function F() {}
function G() {}

function f() {
    for (var i = 0; i < 10000; ++i) {
        var o = Reflect.construct(F, []);
        assertEq(Object.getPrototypeOf(o), F.prototype);
    }

    for (var i = 0; i < 10000; ++i) {
        var o = Reflect.construct(F, [], G);
        assertEq(Object.getPrototypeOf(o), G.prototype);
    }
}

for (var i = 0; i < 2; ++i) f();
