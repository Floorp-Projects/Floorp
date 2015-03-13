var o = {}
Object.defineProperty(o, "p", {
    get: function() {
        return arguments.callee.caller.caller;
    }
});

function f() {
    function g() {
        return o.p;
    }
    return g();
}

for (var k = 0; k < 2; k++) {
    assertEq(f(), f);
}
