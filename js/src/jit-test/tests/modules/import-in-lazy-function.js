// |jit-test| module

// Test that accessing imports in lazily parsed functions works.

import { a } from "module1.js";

function add1(x) {
    return x + a;
}

assertEq(add1(2), 3);
