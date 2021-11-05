// |jit-test| --fast-warmup

function inner() {
    return arguments[100]
}

function outer0() {
    trialInline();
    return inner();
}

function outer1() {
    trialInline();
    return inner(1);
}

function outer2() {
    trialInline();
    return inner(1, 2);
}

function outer3() {
    trialInline();
    return inner(1,2,3)
}

function outer4() {
    trialInline();
    return inner(1,2,3,4)
}

with ({}) {}

for (var i = 0; i < 50; i++) {
    assertEq(outer0(), undefined);
    assertEq(outer1(), undefined);
    assertEq(outer2(), undefined);
    assertEq(outer3(), undefined);
    assertEq(outer4(), undefined);
}
