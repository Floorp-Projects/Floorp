// |jit-test| --ion-warmup-threshold=0; --ion-check-range-analysis

function f(o) {
    // Int32 math does an overflow check.
    o += 1;
    // Int32 math does an underflow check.
    o += -2147483647;
    // If previous math operations are folded, we should keep the smallest
    // overflow check and the highest underflow check to stay within the
    // expected range deduced by Range Analysis.
    for (let i = 0; i < 1; ++i) {
        o -= 1;
    }
}
f(0);
f(0);
f(2147483647);

