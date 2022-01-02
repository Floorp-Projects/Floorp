// |reftest| skip-if(!xulRuntime.shell)
var BUGNUMBER = 1040196;
var summary = 'ToLength';

print(BUGNUMBER + ": " + summary);

var ToLength = getSelfHostedValue('ToLength');

// Negative operands
assertEq(ToLength(-0), 0);
assertEq(ToLength(-1), 0);
assertEq(ToLength(-2), 0);
assertEq(ToLength(-1 * Math.pow(2, 56)), 0);
assertEq(ToLength(-1 * Math.pow(2, 56) - 2), 0);
assertEq(ToLength(-1 * Math.pow(2, 56) - 2.4444), 0);
assertEq(ToLength(-Infinity), 0);

// Small non-negative operands
assertEq(ToLength(0), 0);
assertEq(ToLength(1), 1);
assertEq(ToLength(2), 2);
assertEq(ToLength(3.3), 3);
assertEq(ToLength(10/3), 3);

// Large non-negative operands
var maxLength = Math.pow(2, 53) - 1;
assertEq(ToLength(maxLength - 1), maxLength - 1);
assertEq(ToLength(maxLength - 0.0000001), maxLength);
assertEq(ToLength(maxLength), maxLength);
assertEq(ToLength(maxLength + 0.00000000000001), maxLength);
assertEq(ToLength(maxLength + 1), maxLength);
assertEq(ToLength(maxLength + 2), maxLength);
assertEq(ToLength(Math.pow(2,54)), maxLength);
assertEq(ToLength(Math.pow(2,64)), maxLength);
assertEq(ToLength(Infinity), maxLength);

// NaN operand
assertEq(ToLength(NaN), 0);


reportCompare(0, 0, "ok");
