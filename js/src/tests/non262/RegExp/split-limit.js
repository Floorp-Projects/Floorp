var BUGNUMBER = 1287525;
var summary = "RegExp.prototype[@@split] shouldn't use optimized path if limit is not number.";

print(BUGNUMBER + ": " + summary);

var rx = /a/;
var r = rx[Symbol.split]("abba", {valueOf() {
  RegExp.prototype.exec = () => null;
  return 100;
}});
assertEq(r.length, 1);

if (typeof reportCompare === "function")
    reportCompare(true, true);
