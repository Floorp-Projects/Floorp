assertEq((function(x, y, x) { return (function() { return x+y; })(); })(1,2,5), 7);
