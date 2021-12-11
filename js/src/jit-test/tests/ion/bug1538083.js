// Crashes with --no-threads --ion-eager.
x = [8589934592, -0];
y = [0, 0];
for (let i = 0; i < 2; ++i) {
    y[i] = Math.trunc(Math.tan(x[i]));
}
assertEq(Object.is(y[0], 1), true);
assertEq(Object.is(y[1], -0), true);
