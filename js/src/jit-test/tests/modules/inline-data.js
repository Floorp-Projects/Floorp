// |jit-test| module

import { a } from "javascript: export let a = 42;";
assertEq(a, 42);
