// Arrow right-associativity with =

var a, b, c;
a = b = c => a = b = c;
assertEq(a, b);
a(13);
assertEq(b, 13);
assertEq(a, 13);
