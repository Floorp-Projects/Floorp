// |jit-test| error: ReferenceError

// Test the TDZ works for glbao lexicals through Debugger environments in
// compound assignments.
load(libdir + "evalInFrame.js");

evalInFrame(0, "x |= 0");
let x;
