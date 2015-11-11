// |jit-test| module; error: SyntaxError
import { a } from "module1.js";
delete a;
