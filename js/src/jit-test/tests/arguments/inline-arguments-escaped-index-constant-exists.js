// |jit-test| --fast-warmup

function inner() {
    return arguments
}

function outer0() {
    trialInline();
    return 1 in inner();
}

function outer1() {
    trialInline();
    return 1 in inner(1);
}

function outer2() {
    trialInline();
    return 1 in inner(1, 2);
}

function outer3() {
    trialInline();
    return 1 in inner(1,2,3);
}

function outer4() {
    trialInline();
    return 1 in inner(1,2,3,4);
}

with ({}) {}

for (var i = 0; i < 50; i++) {
    assertEq(outer0(), false);
    assertEq(outer1(), false);
    assertEq(outer2(), true);
    assertEq(outer3(), true);
    assertEq(outer4(), true);
}
