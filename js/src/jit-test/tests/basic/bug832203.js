// Don't assert.
gczeal(2,1);
eval("(function() { " + "\
var g1 = newGlobal('same-compartment');\
function test(str, f) {\
    var x = f(eval(str));\
    assertEq(x, f(g1.eval(str)));\
}\
test('new RegExp(\"1\")', function(r) assertEq('a1'.search(r), 1));\
" + " })();");
eval("(function() { " + "" + " })();");
