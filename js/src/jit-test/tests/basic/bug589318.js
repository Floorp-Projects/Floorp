// Use arguments in an eval.
code = " \
function f(a) { var x = a; } \
for (var i = 0; i < 10; i++) { f(5); } \
";

eval(code);


// Test it doesn't assert.
