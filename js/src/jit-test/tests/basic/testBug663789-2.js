// |jit-test| debug;mjit

o = { toString:function() { return evalInFrame(1, "x") } }
var x = 'C';
var s = "aaaaaaaaaa".replace(/a/g, function() { var x = 'B'; return o });
assertEq(s, "CCCCCCCCCC");
