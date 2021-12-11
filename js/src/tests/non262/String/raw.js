var BUGNUMBER = 1039774;
var summary = 'String.raw';

print(BUGNUMBER + ": " + summary);

assertThrowsInstanceOf(function() { String.raw(); }, TypeError);

assertEq(String.raw.length, 1);

var cooked = [];
assertThrowsInstanceOf(function() { String.raw(cooked); }, TypeError);

cooked.raw = {};
assertEq(String.raw(cooked), "");

cooked.raw = {lengt: 0};
assertEq(String.raw(cooked), "");

cooked.raw = {length: 0};
assertEq(String.raw(cooked), "");

cooked.raw = {length: -1};
assertEq(String.raw(cooked), "");

cooked.raw = [];
assertEq(String.raw(cooked), "");

cooked.raw = ["a"];
assertEq(String.raw(cooked), "a");

cooked.raw = ["a", "b"];
assertEq(String.raw(cooked, "x"), "axb");

cooked.raw = ["a", "b"];
assertEq(String.raw(cooked, "x", "y"), "axb");

cooked.raw = ["a", "b", "c"];
assertEq(String.raw(cooked, "x"), "axbc");

cooked.raw = ["a", "b", "c"];
assertEq(String.raw(cooked, "x", "y"), "axbyc");

cooked.raw = ["\n", "\r\n", "\r"];
assertEq(String.raw(cooked, "x", "y"), "\nx\r\ny\r");

cooked.raw = ["\n", "\r\n", "\r"];
assertEq(String.raw(cooked, "\r\r", "\n"), "\n\r\r\r\n\n\r");

cooked.raw = {length: 2, '0':"a", '1':"b", '2':"c"};
assertEq(String.raw(cooked, "x", "y"), "axb");

cooked.raw = {length: 4, '0':"a", '1':"b", '2':"c"};
assertEq(String.raw(cooked, "x", "y"), "axbycundefined");

reportCompare(0, 0, "ok");
