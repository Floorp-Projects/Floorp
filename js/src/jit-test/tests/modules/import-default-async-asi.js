// |jit-test| module

import v from "export-default-async-asi.js";

assertEq(typeof v, "function");
assertEq(v.name, "async");
assertEq(v(), 17);
