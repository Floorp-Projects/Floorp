// |jit-test| module; error:SyntaxError

export { a } from "empty.js";
export* from "module1.js";
