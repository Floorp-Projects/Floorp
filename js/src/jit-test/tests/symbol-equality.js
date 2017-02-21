setJitCompilerOption("ion.warmup.trigger", 10);

function simpleEquality() {
    for (var i = 0; i < 150; i++) {
        var x = Symbol();
        var y = Symbol();
        assertEq(x === y, false);
        assertEq(x !== y, true);
        assertEq(x == y, false);
        assertEq(x != y, true);
    }
}

function equalOperands() {
    for (var i = 0; i < 150; i++) {
        var x = Symbol();
        assertEq(x === x, true);
        assertEq(x !== x, false);
    }
}

equalOperands();
simpleEquality();
