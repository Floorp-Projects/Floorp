// Test reading and setting values on "hollow" debug scopes. In testGet and
// testSet below, f and g *must* be called from a non-heavyweight lambda to
// trigger the creation of the "hollow" debug scopes for the missing scopes.
//
// The reason is that a direct call to f or g that accesses a in testGet or
// testSet's frame is actually recoverable. The Debugger can synthesize a scope
// based on the frame. By contorting through a lambda, it becomes unsound to
// synthesize a scope based on the lambda function's frame. Since f and g are
// accessing a, which is itself free inside the lambda, the Debugger has no way
// to tell if the on-stack testGet or testSet frame is the frame that *would
// have* allocated a scope for the lambda, *had the lambda been heavyweight*.
// 
// More concretely, if the inner lambda were returned from testGet and testSet,
// then called from a different invocation of testGet or testSet, it becomes
// obvious that it is incorrect to synthesize a scope based on the frame of
// that different invocation.

load(libdir + "evalInFrame.js");

function f() {
  // Eval one frame up. Nothing aliases a.
  evalInFrame(1, "print(a)");
}

function g() {
  evalInFrame(1, "a = 43");
}

function testGet() {
  {
    let a = 42;
    (function () { f(); })();
  }
}

function testSet() {
  {
    let a = 42;
    (function () { g(); })();
  }
}

var log = "";

try {
  testGet();
} catch (e) {
  // Throws due to a having been optimized out.
  log += "g";
}

try {
  testSet();
} catch (e) {
  // Throws due to a having been optimized out.
  log += "s";
}

assertEq(log, "gs");
