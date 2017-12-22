var BUGNUMBER = 1274393;
var summary = "RegExp constructor should check the pattern syntax again when adding unicode flag.";

print(BUGNUMBER + ": " + summary);

assertThrowsInstanceOf(() => RegExp(/\-/, "u"), SyntaxError);

if (typeof reportCompare === "function")
    reportCompare(true, true);
