// |jit-test| module

import { a } from "module1.js";
import { b } from "module2.js";
import { c } from "module3.js";
import d from "module4.js";

assertEq(a, 1);
assertEq(b, 2);
assertEq(c, 3);
assertEq(d, 4);
