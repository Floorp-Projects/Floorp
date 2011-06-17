// |jit-test| debug;mjit

/*
 * NOTE: this evalInFrame is explicitly exposing an optimization artifact that
 * InvokeSessionGuard leaves the callee frame on the stack between invocations.
 * If this ever gets fixed or InvokeSessionGuard gets removed, this test will
 * fail and it can be removed.
 */
o = { toString:function() { return evalInFrame(1, "arguments; x") } }
var s = "aaaaaaaaaa".replace(/a/g, function() { var x = 'B'; return o });
assertEq(s, "BBBBBBBBBB");
