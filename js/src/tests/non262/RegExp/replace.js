var BUGNUMBER = 887016;
var summary = "Implement RegExp.prototype[@@replace].";

print(BUGNUMBER + ": " + summary);

assertEq(RegExp.prototype[Symbol.replace].name, "[Symbol.replace]");
assertEq(RegExp.prototype[Symbol.replace].length, 2);
var desc = Object.getOwnPropertyDescriptor(RegExp.prototype, Symbol.replace);
assertEq(desc.configurable, true);
assertEq(desc.enumerable, false);
assertEq(desc.writable, true);

var re = /a/;
var v = re[Symbol.replace]("abcAbcABC", "X");
assertEq(v, "XbcAbcABC");

re = /d/;
v = re[Symbol.replace]("abcAbcABC", "X");
assertEq(v, "abcAbcABC");

re = /a/ig;
v = re[Symbol.replace]("abcAbcABC", "X");
assertEq(v, "XbcXbcXBC");

re = /(a)(b)(cd)/;
v = re[Symbol.replace]("012abcd345", "_$$_$&_$`_$'_$0_$1_$2_$3_$4_$+_$");
assertEq(v, "012_$_abcd_012_345_$0_a_b_cd_$4_$+_$345");

re = /(a)(b)(cd)/;
v = re[Symbol.replace]("012abcd345", "_\u3042_$$_$&_$`_$'_$0_$1_$2_$3_$4_$+_$");
assertEq(v, "012_\u3042_$_abcd_012_345_$0_a_b_cd_$4_$+_$345");

if (typeof reportCompare === "function")
    reportCompare(true, true);
