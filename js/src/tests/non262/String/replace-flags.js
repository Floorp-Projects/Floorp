var BUGNUMBER = 1108382;
var summary = 'Remove non-standard flag argument from String.prototype.{search,match,replace}.';

printBugNumber(BUGNUMBER);
printStatus (summary);

var result = "bbbAa".match("a", "i");
assertEq(result.index, 4);
assertEq(result.length, 1);
assertEq(result[0], "a");

result = "bbbA".match("a", "i");
assertEq(result, null);

result = "bbbAa".search("a", "i");
assertEq(result, 4);

result = "bbbA".search("a", "i");
assertEq(result, -1);

result = "bbbAaa".replace("a", "b", "g");
assertEq(result, "bbbAba");

if (typeof reportCompare === "function")
  reportCompare(true, true);
