let x = [7];
x.splice(0, {valueOf() { x.length = 2; return 0; }}, 5);
assertEq(x.length, 2);
assertEq(x[0], 5);
assertEq(x[1], 7);
