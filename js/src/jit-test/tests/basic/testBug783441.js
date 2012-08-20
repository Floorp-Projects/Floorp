assertEq((function(x, y, x) { return (function() x+y)(); })(1,2,5), 7);
