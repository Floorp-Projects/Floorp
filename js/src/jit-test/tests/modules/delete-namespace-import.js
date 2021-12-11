// |jit-test| module; error: TypeError
import * as ns from "module1.js";
delete ns.a;
