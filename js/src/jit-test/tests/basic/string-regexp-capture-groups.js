"abcdefg".match(/(x)y(z)/g);
assertEq(RegExp.$1, "");

assertEq("abcdef".match(/a(b)cd/g)[0], "abcd");
assertEq(RegExp.$1, "b");
assertEq(RegExp.$2, "");

"abcdef".match(/(a)b(c)/g);
assertEq(RegExp.$1, "a");
assertEq(RegExp.$2, "c");
assertEq(RegExp.$3, "");

"abcabdabe".match(/(a)b(.)/g);
assertEq(RegExp.$1, "a");
assertEq(RegExp.$2, "e");

"abcdefg".match(/(x)y(z)/g);
assertEq(RegExp.$1, "a");    //If there's no match, we don't update the statics.

"abcdefg".match(/(g)/g);
assertEq(RegExp.$1, "g");
