var BUGNUMBER = 1099956;
var summary =
  "The token next to yield should be tokenized as non-operand if yield is " +
  "a valid name";

printBugNumber(BUGNUMBER);
printStatus(summary);

var yield = 12, a = 3, b = 6, g = 2;
var yieldParsedAsIdentifier = false;

yield /a; yieldParsedAsIdentifier = true; b/g;

assertEq(yieldParsedAsIdentifier, true);

reportCompare(true, true);
