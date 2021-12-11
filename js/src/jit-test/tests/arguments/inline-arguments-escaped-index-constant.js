// |jit-test| --fast-warmup

function inner() {
    return arguments
}

function outer0() {
    trialInline();
    return inner()[0];
}

function outer1() {
    trialInline();
    return inner(1)[0];
}

function outer2() {
    trialInline();
    return inner(1, 2)[0];
}

function outer3() {
    trialInline();
    return inner(1,2,3)[0]
}

function outer4() {
    trialInline();
    return inner(1,2,3,4)[0]
}

with ({}) {}

for (var i = 0; i < 50; i++) {
    assertEq(outer0(), undefined);
    assertEq(outer1(), 1);
    assertEq(outer2(), 1);
    assertEq(outer3(), 1);
    assertEq(outer4(), 1);
}
