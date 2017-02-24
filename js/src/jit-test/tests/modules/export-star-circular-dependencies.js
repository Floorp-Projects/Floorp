// |jit-test| module

import { x, y } from "export-star-circular-1.js";

assertEq(x, "pass");
assertEq(y, "pass");
