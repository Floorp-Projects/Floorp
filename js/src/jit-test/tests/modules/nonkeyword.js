// 'module' is not a keyword in these contexts.

module = {};
module.p = 0;
assertEq(this.module.p, 0);

assertEq(eval('module \n "hello"'), "hello");
assertEq(eval('module \n "world" \n {}'), "world");
