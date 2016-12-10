var BUGNUMBER = 1322319;
var summary = "RegExp.prototype.split should throw if RegRxp.prototype.flags is deleted."

print(BUGNUMBER + ": " + summary);

delete RegExp.prototype.flags;

assertThrowsInstanceOf(() => "aaaaa".split(/a/), SyntaxError);

if (typeof reportCompare === "function")
    reportCompare(true, true);
