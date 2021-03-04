// |jit-test| --fast-warmup

var idx;

function inner() {
    return arguments[idx]
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
    return inner(0,1,2)
}

function outer4() {
    trialInline();
    return inner(0,1,2,3)
}

with ({}) {}

for (idx = 0; idx < 4; idx++) {
    for (var i = 0; i < 50; i++) {
	assertEq(outer0(), idx < 0 ? idx : undefined);
	assertEq(outer1(), idx < 1 ? idx : undefined);
	assertEq(outer2(), idx < 2 ? idx : undefined);
	assertEq(outer3(), idx < 3 ? idx : undefined);
	assertEq(outer4(), idx < 4 ? idx : undefined);
    }
}
