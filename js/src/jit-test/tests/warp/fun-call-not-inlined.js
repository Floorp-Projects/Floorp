// |jit-test| --fast-warmup

// Call a scripted function instead of Function.prototype.call.
function testScriptedAtFunCallOp() {
    var f = function(x) {
        if (x === 130) bailout();
        return x;
    };
    f.call = f;

    for (var i = 0; i < 150; i++) {
        assertEq(f.call(i), i);
    }
}
testScriptedAtFunCallOp();

// Call Function.prototype.call instead of a "normal" function.
function testFunCallAtNormalCallOp() {
    var f = function(x) {
        if (x === 130) bailout();
        return x;
    };
    f.myCall = Function.prototype.call;

    for (var i = 0; i < 150; i++) {
        assertEq(f.myCall(null, i), i);
    }
}
testFunCallAtNormalCallOp();
