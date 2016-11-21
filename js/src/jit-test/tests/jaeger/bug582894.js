// |jit-test| need-for-each

for each(let e in [0x40000000]) {
    (0)[e]
}

/* Don't assert. */

