// |jit-test| --fast-warmup

var dummy;

function inner(x,y) {
    dummy = arguments.length;
    return y;
}

function outer0() {
    trialInline();
    return inner();
}

function outer1() {
    trialInline();
    return inner(0);
}

function outer2() {
    trialInline();
    return inner(0, 1);
}

function outer3() {
    trialInline();
    return inner(0,1,2);
}

with ({}) {}

for (var i = 0; i < 50; i++) {
    assertEq(outer0(), undefined);
    assertEq(outer1(), undefined);
    assertEq(outer2(), 1);
    assertEq(outer3(), 1);
}
