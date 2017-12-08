var BUGNUMBER = 887016;
var summary = "String.prototype.replace should use and update lastIndex if sticky flag is set";

print(BUGNUMBER + ": " + summary);

var input = "abcdeabcdeabcdefghij";
var re = new RegExp("abcde", "y");
re.test(input);
assertEq(re.lastIndex, 5);
var ret = input.replace(re, "ABCDE");
assertEq(ret, "abcdeABCDEabcdefghij");
assertEq(re.lastIndex, 10);
ret = input.replace(re, "ABCDE");
assertEq(ret, "abcdeabcdeABCDEfghij");
assertEq(re.lastIndex, 15);
ret = input.replace(re, "ABCDE");
assertEq(ret, "abcdeabcdeabcdefghij");
assertEq(re.lastIndex, 0);

if (typeof reportCompare === "function")
    reportCompare(true, true);
