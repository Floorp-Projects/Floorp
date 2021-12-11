function f(x,y) {
    return Math.trunc(+(y ? x : y) || ~y);
}

with ({}) {}

for (var i = 0; i < 10; i++) {
    f(0,1);
    f(NaN,1);
    f(0.1,0);
}

assertEq(f(0.1, 1), 0);
