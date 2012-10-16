// Test evaluate's catchTermination option.

var x = 0;
assertEq(evaluate('x = 1; terminate(); x = 2;', { catchTermination: true }),
         "terminated");
assertEq(x, 1);
