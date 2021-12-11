// |jit-test| --fast-warmup; --no-threads

function f(x) {
    (function () {
        (1 == (x & 0) * 1.1) + x;
    })();
}
let y = [,,,2147483648,0,0];
for (let i = 0; i < 6; i++) {
    f(y[i]);
    f(y[i]);
    f(y[i]);
    f(y[i]);
    f(y[i]);
    f(y[i]);
    f(y[i]);
    f(y[i]);
    f(y[i]);
}
