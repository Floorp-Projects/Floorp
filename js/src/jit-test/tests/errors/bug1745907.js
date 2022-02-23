var global;

function foo() {
    var x = BigInt(-1.7e+308);
    try {
        global = BigInt.asUintN(100000000, x);
    } catch {}
}
for (var i = 0; i < 100; i++) {
    foo()
}
assertEq(global, undefined)
