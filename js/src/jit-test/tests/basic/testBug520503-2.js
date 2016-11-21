// |jit-test| need-for-each

for each(let a in [new Boolean(false)]) {}
for (var b = 0; b < 13; ++b) {
    if (b % 3 == 1) {
        (function f(c) {
            if (c <= 1) {
                return 1;
            }
            return f(c - 1) + f(c - 2);
        })(3)
    } else {
        (function g(d, e) {;
            return d.length == e ? 0 : d[e] + g(d, e + 1);
        })([false, new Boolean(true), false], 0)
    }
}

/* Should not have crashed. */

