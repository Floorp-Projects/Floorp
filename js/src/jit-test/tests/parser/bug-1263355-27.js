// |jit-test| error: ReferenceError

load(libdir + "evalInFrame.js");
evalInFrame(1, "a = 43");
let a = 42;
