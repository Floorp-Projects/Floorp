// arrow functions may have empty arguments

var f = () => 'x';
assertEq(f.length, 0);
assertEq(f(), 'x');
assertEq(f(0, 1, 2, 3, 4, 5, 6, 7, 8, 9), 'x');
