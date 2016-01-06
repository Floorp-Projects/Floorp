// |jit-test| module

import c from "defaultClass.js";
let o = new c();
assertEq(o.triple(14), 42);
