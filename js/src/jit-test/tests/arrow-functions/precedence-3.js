// => binds tighter than , (on the other side)

var h = (a => a, 13);  // sequence expression
assertEq(h, 13);
