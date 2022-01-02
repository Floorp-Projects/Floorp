// |jit-test| module

import * as ns from 'module1.js';

let other = ns;

const iterations = 10000;

let x = 0;
for (let i = 0; i < iterations; i++) {
    if (i == iterations / 2)
        other = { a: 0 };
    x += other.a;
}

assertEq(other.a, 0);
assertEq(x, iterations / 2);
