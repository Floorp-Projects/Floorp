// Test that the tracer is not confused by a.m() when a is the same shape each
// time through the loop but a.m is a scripted getter that returns different
// functions.

function f() { return 'f'; }
function g() { return 'g'; }
var arr = [f, f, f, f, f, f, f, f, g];
var a = {get m() { return arr[i]; }};

var s = '';
for (var i = 0; i < 9; i++)
    s += a.m();
assertEq(s, 'ffffffffg');
