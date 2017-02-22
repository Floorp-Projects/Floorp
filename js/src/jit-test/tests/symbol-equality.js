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

function bitwiseCompare() {
    var ar = [true, false, Symbol(), null, undefined];
    var s = Symbol();
    ar.push(s);

    for (var i = 0; i < 150; i++) {
        for (var j = 0; j < ar.length; j++) {
            var equal = (j == ar.indexOf(s));

            assertEq(ar[j] === s, equal);
            assertEq(ar[j] !== s, !equal);
            assertEq(ar[j] == s, equal);
            assertEq(ar[j] != s, !equal);
        }
    }
}

equalOperands();
simpleEquality();
bitwiseCompare();
