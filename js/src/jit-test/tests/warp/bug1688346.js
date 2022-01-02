function f(x) {
    let y = Math.trunc(x);
    return y - y;
}

with ({}) {}

for (var i = 0; i < 50; i++) {
    f(0.1);
}

assertEq(f(NaN), NaN);
