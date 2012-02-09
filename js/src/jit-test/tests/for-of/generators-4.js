// for-of can iterate over generator-iterators produced by generator-expressions.

function g() {
    yield 1;
    yield 2;
}

var it = g();
for (var i = 0; i < 10; i++) {
    let prev = it;
    it = (k + 1 for (k of prev));
}

var arr = [v for (v of it)];
assertEq(arr.join(), "11,12");
