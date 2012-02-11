var h = { apply:function() { return "ponies"; } };
function f() { var x = 3; g = function() { return ++x }; return h.apply(null, arguments) }
assertEq(f(), "ponies");
