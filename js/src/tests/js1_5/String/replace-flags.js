// |reftest| skip-if(!xulRuntime.shell)

var BUGNUMBER = 1142351;
var summary = 'Add console warnings for non-standard flag argument of String.prototype.{search,match,replace}.';

printBugNumber(BUGNUMBER);
printStatus (summary);

options("werror");
assertEq(evaluate("'aaaA'.match('a', 'i')", {catchTermination: true}), "terminated");
assertEq(evaluate("'aaaA'.search('a', 'i')", {catchTermination: true}), "terminated");
assertEq(evaluate("'aaaA'.replace('a', 'b', 'g')", {catchTermination: true}), "terminated");

if (typeof reportCompare === "function")
  reportCompare(true, true);
