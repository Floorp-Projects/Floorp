// Crashes with --no-threads --ion-eager.
x = [8589934592, -0];
y = [0, 0];
for (let i = 0; i < 2; ++i) {
    y[i] = uneval(Math.trunc(Math.tan(x[i])));
}
assertEq(y[0].toString(), "1");
assertEq(y[1].toString(), "-0");
