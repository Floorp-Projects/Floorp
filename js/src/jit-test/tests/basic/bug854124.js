// Don't assert.
"p".match(/(p)/);
assertEq(RegExp.$1, "p");
assertEq(RegExp.$2, "");

"x\ny\n".replace(/(^\n*)/, "");
assertEq(RegExp.$1, "");
assertEq(RegExp.$2, "");
